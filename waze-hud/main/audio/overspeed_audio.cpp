#include "audio/overspeed_audio.h"

#include "sdkconfig.h"

#if CONFIG_WAZE_HUD_BOARD_ES3C28P && CONFIG_WAZE_HUD_AUDIO_ALERT

#include "audio/overspeed_voice.h"
#include "config/device_config.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "state/hud_state_store.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace waze_hud {
namespace {
constexpr char kTag[] = "AUDIO";

// ES3C28P: ES8311 codec on the shared touch/expansion I2C bus, I2S audio bus
// and an amplifier whose enable pin is active-low.
constexpr gpio_num_t kAmpEnable = GPIO_NUM_1;
constexpr gpio_num_t kMclk = GPIO_NUM_4;
constexpr gpio_num_t kBclk = GPIO_NUM_5;
constexpr gpio_num_t kWs = GPIO_NUM_7;
constexpr gpio_num_t kDout = GPIO_NUM_8;
constexpr gpio_num_t kSda = GPIO_NUM_16;
constexpr gpio_num_t kScl = GPIO_NUM_15;
constexpr i2c_port_t kI2cPort = I2C_NUM_0;
constexpr uint16_t kCodecAddresses[] = {ES8311_ADDRRES_0, 0x19};

constexpr int kSampleRate = 16000;
constexpr int kMclkHz = kSampleRate * 256;
constexpr int kFramesPerChunk = 256;
constexpr float kPi = 3.14159265F;

constexpr TickType_t kDebounceTicks = pdMS_TO_TICKS(1500);
constexpr TickType_t kRepeatTicks = pdMS_TO_TICKS(CONFIG_WAZE_HUD_AUDIO_REPEAT_SEC * 1000);

i2s_chan_handle_t txChannel = nullptr;

float volumeScale() { return 0.85F * CONFIG_WAZE_HUD_AUDIO_VOLUME / 100.0F; }

esp_err_t initCodec() {
    i2c_config_t bus{};
    bus.mode = I2C_MODE_MASTER;
    bus.sda_io_num = kSda;
    bus.scl_io_num = kScl;
    bus.sda_pullup_en = GPIO_PULLUP_ENABLE;
    bus.scl_pullup_en = GPIO_PULLUP_ENABLE;
    bus.master.clk_speed = 100000;
    ESP_RETURN_ON_ERROR(i2c_param_config(kI2cPort, &bus), kTag, "I2C config failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(kI2cPort, bus.mode, 0, 0, 0), kTag,
                        "I2C driver install failed");

    es8311_clock_config_t clock{};
    clock.mclk_inverted = false;
    clock.mclk_from_mclk_pin = true;
    clock.mclk_frequency = kMclkHz;
    clock.sample_frequency = kSampleRate;

    for (const uint16_t address : kCodecAddresses) {
        es8311_handle_t codec = es8311_create(kI2cPort, address);
        if (!codec) continue;
        if (es8311_init(codec, &clock, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
            es8311_delete(codec);
            continue;
        }
        (void)es8311_sample_frequency_config(codec, kMclkHz, kSampleRate);
        (void)es8311_voice_volume_set(codec, 90, nullptr);
        ESP_LOGI(kTag, "ES8311 ready at I2C 0x%02X", static_cast<unsigned>(address));
        return ESP_OK;
    }
    ESP_LOGE(kTag, "ES8311 not found on I2C (tried 0x18 and 0x19)");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t initI2s() {
    i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    channel.auto_clear = true;  // Output silence when no data is queued.
    ESP_RETURN_ON_ERROR(i2s_new_channel(&channel, &txChannel, nullptr), kTag,
                        "I2S channel creation failed");

    i2s_std_config_t config{};
    config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate);
    config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                          I2S_SLOT_MODE_STEREO);
    config.gpio_cfg.mclk = kMclk;
    config.gpio_cfg.bclk = kBclk;
    config.gpio_cfg.ws = kWs;
    config.gpio_cfg.dout = kDout;
    config.gpio_cfg.din = I2S_GPIO_UNUSED;
    ESP_RETURN_ON_ERROR(i2s_channel_init_std_mode(txChannel, &config), kTag,
                        "I2S std mode init failed");
    return i2s_channel_enable(txChannel);
}

void writeFrames(const int16_t *mono, std::size_t count) {
    std::array<int16_t, kFramesPerChunk * 2> chunk{};
    std::size_t done = 0;
    while (done < count) {
        const std::size_t frames = std::min<std::size_t>(kFramesPerChunk, count - done);
        for (std::size_t i = 0; i < frames; ++i) {
            chunk[2 * i] = mono[done + i];
            chunk[2 * i + 1] = mono[done + i];
        }
        std::size_t written = 0;
        (void)i2s_channel_write(txChannel, chunk.data(), frames * 2 * sizeof(int16_t), &written,
                                pdMS_TO_TICKS(500));
        done += frames;
    }
}

void playTone(float frequencyHz, int durationMs) {
    const std::size_t total = static_cast<std::size_t>(kSampleRate) * durationMs / 1000;
    const float amplitude = 32767.0F * volumeScale();
    std::array<int16_t, kFramesPerChunk> samples{};
    std::size_t index = 0;
    while (index < total) {
        const std::size_t count = std::min<std::size_t>(kFramesPerChunk, total - index);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t n = index + i;
            // 5 ms linear fade in/out removes clicks at tone edges.
            const std::size_t fade = kSampleRate / 200;
            float envelope = 1.0F;
            if (n < fade) envelope = static_cast<float>(n) / fade;
            else if (total - n < fade) envelope = static_cast<float>(total - n) / fade;
            samples[i] = static_cast<int16_t>(
                amplitude * envelope * std::sin(2.0F * kPi * frequencyHz * n / kSampleRate));
        }
        writeFrames(samples.data(), count);
        index += count;
    }
}

void playSilence(int durationMs) {
    const std::size_t total = static_cast<std::size_t>(kSampleRate) * durationMs / 1000;
    std::array<int16_t, kFramesPerChunk> zeros{};
    for (std::size_t done = 0; done < total; done += kFramesPerChunk)
        writeFrames(zeros.data(), std::min<std::size_t>(kFramesPerChunk, total - done));
}

#if CONFIG_WAZE_HUD_AUDIO_USE_VOICE
void playVoice() {
    const float scale = volumeScale();
    std::array<int16_t, kFramesPerChunk> samples{};
    for (std::size_t done = 0; done < kOverspeedVoiceSamples; done += kFramesPerChunk) {
        const std::size_t count =
            std::min<std::size_t>(kFramesPerChunk, kOverspeedVoiceSamples - done);
        for (std::size_t i = 0; i < count; ++i)
            samples[i] = static_cast<int16_t>(kOverspeedVoice[done + i] * scale);
        writeFrames(samples.data(), count);
    }
}
#endif

void playAlert() {
    gpio_set_level(kAmpEnable, 0);  // Active-low enable.
    vTaskDelay(pdMS_TO_TICKS(30));
#if CONFIG_WAZE_HUD_AUDIO_USE_VOICE
    if (kOverspeedVoiceSamples > 0) {
        playVoice();
    } else
#endif
    {
        playTone(1000.0F, 160);
        playSilence(90);
        playTone(1400.0F, 160);
        playSilence(90);
        playTone(1000.0F, 160);
    }
    playSilence(60);
    gpio_set_level(kAmpEnable, 1);
}

bool isOverspeed() {
    const HudState state = HudStateStore::instance().snapshot();
    const DeviceSettings settings = DeviceConfig::instance().snapshot();
    const int threshold =
        std::max(0, state.speedLimitKmh + static_cast<int>(settings.overspeedOffsetKmh));
    return state.connected && state.hasProducerState && !state.signalStale &&
           state.speedLimitKmh > 0 && state.speedKmh > threshold;
}

void audioTask(void *) {
    TickType_t overspeedSince = 0;
    TickType_t lastAlert = 0;
    bool alerted = false;
    for (;;) {
        if (isOverspeed()) {
            const TickType_t now = xTaskGetTickCount();
            if (overspeedSince == 0) overspeedSince = now;
            const bool debounced = now - overspeedSince >= kDebounceTicks;
            if (debounced && (!alerted || now - lastAlert >= kRepeatTicks)) {
                playAlert();
                alerted = true;
                lastAlert = xTaskGetTickCount();
            }
        } else {
            overspeedSince = 0;
            alerted = false;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
}  // namespace

esp_err_t startOverspeedAudio() {
    gpio_config_t amp{};
    amp.pin_bit_mask = 1ULL << static_cast<unsigned>(kAmpEnable);
    amp.mode = GPIO_MODE_OUTPUT;
    ESP_RETURN_ON_ERROR(gpio_config(&amp), kTag, "Amplifier enable setup failed");
    gpio_set_level(kAmpEnable, 1);  // Amplifier off until an alert plays.

    ESP_RETURN_ON_ERROR(initI2s(), kTag, "I2S init failed");  // Provides MCLK for the codec.
    ESP_RETURN_ON_ERROR(initCodec(), kTag, "Codec init failed");
    const BaseType_t created = xTaskCreate(audioTask, "overspeed_audio", 4096, nullptr, 2, nullptr);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

}  // namespace waze_hud

#else

namespace waze_hud {
esp_err_t startOverspeedAudio() { return ESP_ERR_NOT_SUPPORTED; }
}  // namespace waze_hud

#endif
