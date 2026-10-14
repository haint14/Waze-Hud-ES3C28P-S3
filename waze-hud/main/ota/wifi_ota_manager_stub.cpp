// ESP32-S3 (ES3C28P) replacement for ota/wifi_ota_manager.cpp.
//
// The upstream WiFi Web OTA opens an unauthenticated firmware-upload page on a
// SoftAP with a fixed password and is triggered by the CYD touch/BOOT hold.
// It is not enabled for this board: the S3 is flashed over USB-C, which also
// keeps WiFi out of the firmware. The stub reports "OTA never active" so the
// shared UI code compiles unchanged.
#include "ota/wifi_ota_manager.h"

namespace waze_hud {

WifiOtaManager &WifiOtaManager::instance() {
    static WifiOtaManager manager;
    return manager;
}

esp_err_t WifiOtaManager::start() { return ESP_ERR_NOT_SUPPORTED; }

void WifiOtaManager::stop() {}

bool WifiOtaManager::isActive() const { return false; }

WifiOtaSnapshot WifiOtaManager::snapshot() const { return {}; }

void WifiOtaManager::updateProgress(uint8_t) {}

void WifiOtaManager::setStatus(OtaStatus, const char *) {}

void WifiOtaManager::tickSecond() {}

}  // namespace waze_hud
