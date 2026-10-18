#pragma once

#include "cJSON.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"
#include <cstdint>

namespace waze_hud {

enum class UiTheme : uint8_t { Auto, Day, Night };

// Factory default for LCD inversion. The ES3C28P IPS panel needs INVON; the
// user can still flip it from the app (invert_color).
#ifdef CONFIG_WAZE_HUD_LCD_INVERT_COLOR
constexpr bool kDefaultInvertColor = true;
#else
constexpr bool kDefaultInvertColor = false;
#endif

// Factory default for the 180-degree rotation (CONFIG_WAZE_HUD_DEFAULT_ROTATE_DISPLAY). Upstream's
// CYD default is on (USB on the right); the ES3C28P is mounted USB-left, the unrotated orientation.
#ifdef CONFIG_WAZE_HUD_DEFAULT_ROTATE_DISPLAY
constexpr bool kDefaultRotateDisplay = true;
#else
constexpr bool kDefaultRotateDisplay = false;
#endif

struct DeviceSettings {
    bool fixedBrightness{false};
    uint8_t brightness{100};
    UiTheme theme{UiTheme::Auto};
    bool invertColor{kDefaultInvertColor};
    bool mirrorHud{false};
    bool rotateDisplay{kDefaultRotateDisplay};
    int8_t overspeedOffsetKmh{0};
    int8_t offsetX{0};
    int8_t offsetY{0};
    uint32_t revision{7};
};

using HlpSendLine = void (*)(const char *line, void *context);

class DeviceConfig {
public:
    static DeviceConfig &instance();

    esp_err_t init();
    DeviceSettings snapshot() const;
    esp_err_t toggleRotation();
    esp_err_t toggleMirror();
    bool handleMessage(const cJSON *root, HlpSendLine send, void *context);
    void publishSchema(HlpSendLine send, void *context);

private:
    DeviceConfig() = default;
    mutable portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    DeviceSettings active_{};
};

}  // namespace waze_hud
