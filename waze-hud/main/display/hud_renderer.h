#pragma once

#include "config/device_config.h"
#include "display/bitmap_font.h"
#include "display/layout.h"
#include "state/hud_state.h"
#include "system/system_status.h"
#include "esp_err.h"
#include "ota/wifi_ota_manager.h"
#include "touch/touch_sensor.h"
#include <cstdint>

namespace waze_hud {

class HudRenderer {
public:
    esp_err_t init();
    void render(const HudState &state, const DeviceSettings &settings,
                const SystemStatusSnapshot &systemStatus);
    bool animationActive() const {
        return marqueeActive_ || clockActive_ || nextStreetMarqueeActive_ || overspeedActive_ ||
               previousOtaActive_ || previousOtaConfirming_;
    }

private:
    void renderRegion(const Rect &region, const HudState &state, const DeviceSettings &settings,
                      const SystemStatusSnapshot &systemStatus);
    void renderOtaScreen(Canvas &canvas, const Rect &region, const WifiOtaSnapshot &ota);
    void renderOtaConfirm(Canvas &canvas, const Rect &region, const TouchOtaConfirmStatus &confirm);
    void renderSystemStatus(Canvas &canvas, const Rect &region,
                            const SystemStatusSnapshot &systemStatus,
                            const DeviceSettings &settings);
    void renderMainIndicators(Canvas &canvas, const Rect &region,
                              const SystemStatusSnapshot &systemStatus);
    void renderStatus(Canvas &canvas, const Rect &region, const HudState &state, const DeviceSettings &settings);
    void renderMainCluster(Canvas &canvas, const HudState &state, const DeviceSettings &settings);
    void renderManeuver(Canvas &canvas, const HudState &state, const DeviceSettings &settings);
    void renderSpeedCluster(Canvas &canvas, const HudState &state, const DeviceSettings &settings);
    void renderAlerts(Canvas &canvas, const HudState &state, const DeviceSettings &settings);
    void renderGuidance(Canvas &canvas, const HudState &state, const DeviceSettings &settings);
    void renderStreet(Canvas &canvas, const HudState &state, const DeviceSettings &settings);

    uint16_t *buffer_{nullptr};
    HudState previous_{};
    DeviceSettings previousSettings_{};
    SystemStatusSnapshot previousSystemStatus_{};
    int64_t renderedClockMinute_{INT64_MIN};
    int8_t renderedClockPhase_{-1};
    uint64_t marqueeEpochMs_{0};
    int marqueeOffset_{0};
    int marqueeRenderedOffset_{-1};
    int marqueeTextWidth_{0};
    int marqueeAvailableWidth_{0};
    bool marqueeActive_{false};
    uint64_t nextStreetMarqueeEpochMs_{0};
    int nextStreetMarqueeOffset_{0};
    int nextStreetMarqueeRenderedOffset_{-1};
    int nextStreetMarqueeTextWidth_{0};
    bool nextStreetMarqueeActive_{false};
    bool overspeedActive_{false};
    bool renderedOverspeedPhase_{false};
    bool clockActive_{false};
    bool firstFrame_{true};
    uint8_t currentAppliedBrightness_{0};
    bool currentAppliedInvert_{false};
    bool previousOtaActive_{false};
    bool previousOtaConfirming_{false};
    uint8_t previousOtaProgress_{0};
    uint16_t previousOtaTimeout_{0};
    uint8_t previousConfirmSeconds_{0};
    uint8_t previousConfirmHold_{0};
};

}  // namespace waze_hud
