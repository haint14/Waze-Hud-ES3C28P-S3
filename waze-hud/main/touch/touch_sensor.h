#pragma once

#include "esp_err.h"
#include <cstdint>

namespace waze_hud {

struct TouchOtaConfirmStatus {
    bool confirming{false};
    uint8_t remainingSeconds{0}; // Đếm ngược cửa sổ xác nhận (5s -> 0s)
    uint8_t holdPercent{0};      // Tiến trình giữ lần 2 (0% -> 100%)
};

class TouchSensor {
public:
    static TouchSensor &instance();

    esp_err_t init();
    TouchOtaConfirmStatus confirmStatus() const;
    bool isTouched() const;

private:
    TouchSensor() = default;
    void runLoop();
    static void taskEntry(void *arg);

    TouchOtaConfirmStatus status_{};
};

}  // namespace waze_hud

