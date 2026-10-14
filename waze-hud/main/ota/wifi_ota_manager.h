#pragma once

#include "esp_err.h"
#include <cstdint>

namespace waze_hud {

enum class OtaStatus : uint8_t {
    Idle,
    WaitingConnection,
    Writing,
    Success,
    Error
};

struct WifiOtaSnapshot {
    bool active{false};
    OtaStatus status{OtaStatus::Idle};
    uint8_t progress{0};            // 0 - 100%
    uint16_t timeoutRemaining{180}; // Đếm ngược giây tự thoát
    char message[64]{0};
};

class WifiOtaManager {
public:
    static WifiOtaManager &instance();

    esp_err_t start();
    void stop();

    bool isActive() const;
    WifiOtaSnapshot snapshot() const;

    void updateProgress(uint8_t percent);
    void setStatus(OtaStatus status, const char *msg = nullptr);
    void tickSecond();

private:
    WifiOtaManager() = default;
    void startAp();
    void stopAp();
    void startHttpServer();
    void stopHttpServer();

    bool active_{false};
    OtaStatus status_{OtaStatus::Idle};
    uint8_t progress_{0};
    uint16_t timeout_{180};
    char message_[64]{0};
    void *httpd_{nullptr};
    void *apNetif_{nullptr};
};

}  // namespace waze_hud

