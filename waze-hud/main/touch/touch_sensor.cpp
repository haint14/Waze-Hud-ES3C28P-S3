#include "touch/touch_sensor.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ota/wifi_ota_manager.h"
#include "state/hud_state_store.h"
#include <algorithm>

namespace waze_hud {
namespace {

constexpr char kTag[] = "TOUCH";

#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
constexpr gpio_num_t kTouchIrq = GPIO_NUM_36;
constexpr gpio_num_t kTouchCs = GPIO_NUM_33;
#endif
constexpr gpio_num_t kBootButton = GPIO_NUM_0;

constexpr TickType_t kPollDelay = pdMS_TO_TICKS(20);
constexpr uint32_t kFirstHoldMs = 3000;
constexpr uint32_t kConfirmWindowMs = 6000;
constexpr uint32_t kSecondHoldMs = 3000;

}  // namespace

TouchSensor &TouchSensor::instance() {
    static TouchSensor sSensor;
    return sSensor;
}

esp_err_t TouchSensor::init() {
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    gpio_config_t csCfg{};
    csCfg.pin_bit_mask = 1ULL << static_cast<unsigned>(kTouchCs);
    csCfg.mode = GPIO_MODE_OUTPUT;
    csCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    csCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    csCfg.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&csCfg), kTag, "Touch CS pin setup failed");
    gpio_set_level(kTouchCs, 1);

    gpio_config_t irqCfg{};
    irqCfg.pin_bit_mask = 1ULL << static_cast<unsigned>(kTouchIrq);
    irqCfg.mode = GPIO_MODE_INPUT;
    irqCfg.pull_up_en = GPIO_PULLUP_DISABLE;
    irqCfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    irqCfg.intr_type = GPIO_INTR_DISABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&irqCfg), kTag, "Touch IRQ pin setup failed");
#endif

    const BaseType_t created = xTaskCreate(taskEntry, "hud_touch", 3072,
                                           this, 2, nullptr);
    return created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool TouchSensor::isTouched() const {
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    if (gpio_get_level(kTouchIrq) == 0) return true;
#endif
    return (gpio_get_level(kBootButton) == 0);
}

TouchOtaConfirmStatus TouchSensor::confirmStatus() const {
    return status_;
}

void TouchSensor::taskEntry(void *arg) {
    auto *self = static_cast<TouchSensor *>(arg);
    self->runLoop();
    vTaskDelete(nullptr);
}

void TouchSensor::runLoop() {
    enum class State {
        Idle,
        FirstHold,
        ConfirmWindow,
        SecondHold
    };

    State state = State::Idle;
    uint32_t holdStartMs = 0;
    uint32_t windowStartMs = 0;

    for (;;) {
        const bool touched = isTouched();
        const uint32_t nowMs = xTaskGetTickCount() * portTICK_PERIOD_MS;

        switch (state) {
            case State::Idle:
                status_.confirming = false;
                status_.holdPercent = 0;
                status_.remainingSeconds = 0;
                if (touched) {
                    holdStartMs = nowMs;
                    state = State::FirstHold;
                }
                break;

            case State::FirstHold:
                if (!touched) {
                    state = State::Idle;
                } else if (nowMs - holdStartMs >= kFirstHoldMs) {
                    ESP_LOGI(kTag, "First 3s hold detected! Entering OTA confirmation window...");
                    windowStartMs = nowMs;
                    state = State::ConfirmWindow;
                    status_.confirming = true;
                    status_.remainingSeconds = static_cast<uint8_t>(kConfirmWindowMs / 1000);
                    status_.holdPercent = 0;
                    HudStateStore::instance().refresh();
                }
                break;

            case State::ConfirmWindow: {
                const uint32_t elapsed = nowMs - windowStartMs;
                if (elapsed >= kConfirmWindowMs) {
                    ESP_LOGI(kTag, "OTA confirmation window expired; resuming normal HUD.");
                    state = State::Idle;
                    status_.confirming = false;
                    HudStateStore::instance().refresh();
                } else {
                    status_.remainingSeconds = static_cast<uint8_t>((kConfirmWindowMs - elapsed + 999) / 1000);
                    if (touched) {
                        holdStartMs = nowMs;
                        state = State::SecondHold;
                    }
                }
                break;
            }

            case State::SecondHold: {
                const uint32_t elapsedWindow = nowMs - windowStartMs;
                if (elapsedWindow >= kConfirmWindowMs) {
                    state = State::Idle;
                    status_.confirming = false;
                    HudStateStore::instance().refresh();
                    break;
                }
                status_.remainingSeconds = static_cast<uint8_t>((kConfirmWindowMs - elapsedWindow + 999) / 1000);

                if (!touched) {
                    status_.holdPercent = 0;
                    state = State::ConfirmWindow;
                    HudStateStore::instance().refresh();
                } else {
                    const uint32_t holdElapsed = nowMs - holdStartMs;
                    status_.holdPercent = static_cast<uint8_t>(std::min<uint32_t>(100U, (holdElapsed * 100U) / kSecondHoldMs));
                    HudStateStore::instance().refresh();

                    if (holdElapsed >= kSecondHoldMs) {
                        ESP_LOGW(kTag, "Second 3s hold confirmed! Launching WiFi Web OTA...");
                        status_.confirming = false;
                        status_.holdPercent = 100;
                        HudStateStore::instance().refresh();

                        xTaskCreate([](void *) {
                            WifiOtaManager::instance().start();
                            vTaskDelete(nullptr);
                        }, "ota_starter", 4096, nullptr, 5, nullptr);

                        vTaskDelay(pdMS_TO_TICKS(150));
                        // Trong khi OTA chạy, tạm dừng vòng lặp touch
                        while (WifiOtaManager::instance().isActive()) {
                            vTaskDelay(pdMS_TO_TICKS(500));
                        }
                        state = State::Idle;
                    }
                }
                break;
            }
        }

        vTaskDelay(kPollDelay);
    }
}

}  // namespace waze_hud
