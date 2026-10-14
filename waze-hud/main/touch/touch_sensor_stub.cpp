// ESP32-S3 (ES3C28P) replacement for touch/touch_sensor.cpp.
//
// The upstream module drives the CYD's XPT2046 resistive touch (IRQ GPIO36,
// CS GPIO33). On the ESP32-S3 with octal PSRAM, GPIO33..37 belong to the PSRAM
// bus, so that code must not run here. The ES3C28P touch panel is a different
// chip (FT6336G over I2C shared with the audio codec) and is not used yet.
// This stub keeps the UI code compiling and always reports "no OTA confirm".
#include "touch/touch_sensor.h"

namespace waze_hud {

TouchSensor &TouchSensor::instance() {
    static TouchSensor sensor;
    return sensor;
}

esp_err_t TouchSensor::init() { return ESP_OK; }

TouchOtaConfirmStatus TouchSensor::confirmStatus() const { return {}; }

bool TouchSensor::isTouched() const { return false; }

}  // namespace waze_hud
