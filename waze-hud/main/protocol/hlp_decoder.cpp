#include "protocol/hlp_decoder.h"

#include "protocol/hlp_core.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace waze_hud {
namespace {
constexpr char kTag[] = "HLP";

int integerOr(const cJSON *root, const char *key, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble) return fallback;
    return static_cast<int>(item->valuedouble);
}

float numberOr(const cJSON *root, const char *key, float fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsNumber(item) && std::isfinite(item->valuedouble)
        ? static_cast<float>(item->valuedouble) : fallback;
}

uint32_t unsigned32Or(const cJSON *root, const char *key, uint32_t fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble || item->valuedouble < 0.0 ||
        item->valuedouble > 4294967295.0) return fallback;
    return static_cast<uint32_t>(item->valuedouble);
}

int64_t integer64Or(const cJSON *root, const char *key, int64_t fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) ||
        std::floor(item->valuedouble) != item->valuedouble ||
        item->valuedouble < -9007199254740991.0 || item->valuedouble > 9007199254740991.0)
        return fallback;
    return static_cast<int64_t>(item->valuedouble);
}

template <std::size_t Capacity>
void copyUtf8(const cJSON *root, const char *key, std::array<char, Capacity> &destination) {
    destination.fill(0);
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!cJSON_IsString(item) || !item->valuestring) return;
    std::size_t count = std::min<std::size_t>(std::strlen(item->valuestring), Capacity - 1);
    while (count > 0 && !hlp_utf8_valid(reinterpret_cast<const uint8_t *>(item->valuestring), count)) --count;
    std::memcpy(destination.data(), item->valuestring, count);
}

// Waze phrases a straight-ahead instruction as "Tiếp tục đi thẳng ..."; "Đi thẳng ..." reads faster on
// a HUD. The replacement is shorter than every prefix, so it is rewritten in place.
template <std::size_t Capacity>
void shortenContinueStraight(std::array<char, Capacity> &text) {
    static constexpr char kReplacement[] = "Đi thẳng";
    static constexpr const char *kPrefixes[] = {"Tiếp tục đi thẳng", "tiếp tục đi thẳng",
                                                "TIẾP TỤC ĐI THẲNG"};
    for (const char *prefix : kPrefixes) {
        const std::size_t prefixLength = std::strlen(prefix);
        if (std::strncmp(text.data(), prefix, prefixLength) != 0) continue;
        const char next = text[prefixLength];
        if (next != '\0' && next != ' ') continue;
        const std::size_t replacementLength = sizeof(kReplacement) - 1;
        const std::size_t restLength = std::strlen(text.data() + prefixLength);
        std::memmove(text.data() + replacementLength, text.data() + prefixLength, restLength + 1);
        std::memcpy(text.data(), kReplacement, replacementLength);
        return;
    }
}

Maneuver maneuverValue(int value) {
    return value >= 0 && value <= 20 ? static_cast<Maneuver>(value) : Maneuver::None;
}

AlertKind alertValue(int value) {
    if (value == 0) return AlertKind::None;
    if (value > 0 && value <= 75) return static_cast<AlertKind>(value);
    return value > 0 ? AlertKind::Hazard : AlertKind::None;
}

void decodeTrafficDetails(const cJSON *object, const char *severityKey,
                          const char *delayKey, AlertState &alert) {
    if (alert.kind != AlertKind::TrafficJam) {
        alert.trafficSeverity = 0;
        alert.trafficDelayMinutes = -1;
        return;
    }
    const int severity = integerOr(object, severityKey, 0);
    alert.trafficSeverity = severity >= 1 && severity <= 5
        ? static_cast<uint8_t>(severity) : 0;
    const int delay = integerOr(object, delayKey, -1);
    alert.trafficDelayMinutes = delay >= 0 ? delay : -1;
}

bool isProhibitionAlert(AlertKind kind) {
    switch (kind) {
        case AlertKind::NoPassing:
        case AlertKind::EndNoPassing:
        case AlertKind::NoCar:
        case AlertKind::NoMotorcycle:
        case AlertKind::NoLeftTurn:
        case AlertKind::NoRightTurn:
        case AlertKind::NoUTurn:
        case AlertKind::NoStraight:
        case AlertKind::MandatoryStraight:
        case AlertKind::MandatoryRight:
        case AlertKind::MandatoryLeft:
        case AlertKind::CarLane:
        case AlertKind::MotorcycleLane:
        case AlertKind::OneWay:
        case AlertKind::ProhibitedRoad:
        case AlertKind::CombinedTurnRestriction:
        case AlertKind::NoStraightAndRight:
        case AlertKind::NoLeftAndUTurn:
        case AlertKind::NoStraightAndLeft:
        case AlertKind::NoLeftAndRight:
        case AlertKind::CarNoLeftAndUTurn:
        case AlertKind::CarNoRightAndUTurn:
        case AlertKind::NoRightAndUTurn:
        case AlertKind::CarNoLeftTurn:
        case AlertKind::CarNoRightTurn:
        case AlertKind::CarNoUTurn:
        case AlertKind::EndAllProhibitions:
        case AlertKind::EndSpeedRestriction:
            return true;
        default:
            return false;
    }
}

bool isCameraAlert(AlertKind kind) {
    switch (kind) {
        case AlertKind::SpeedCamera:
        case AlertKind::RedLightCamera:
        case AlertKind::PhoneCamera:
        case AlertKind::DummyCamera:
        case AlertKind::SeatbeltCamera:
        case AlertKind::DistanceCamera:
        case AlertKind::BusLaneCamera:
        case AlertKind::NoiseCamera:
        case AlertKind::StopSignCamera:
            return true;
        default:
            return false;
    }
}

int alertPriorityScore(const AlertState &alert) {
    if (alert.kind == AlertKind::SpeedDrop) return 100;
    if (isProhibitionAlert(alert.kind)) return 90;
    if (alert.kind == AlertKind::RoadClosed || alert.kind == AlertKind::Accident ||
        alert.kind == AlertKind::Hazard) return 70;
    if (alert.kind == AlertKind::TrafficLight) {
        if (alert.distanceM >= 0 && alert.distanceM <= 250) return 60;
        return 20;
    }
    if (alert.kind == AlertKind::TrafficJam) return 50;
    if (isCameraAlert(alert.kind)) {
        if (alert.distanceM >= 0 && alert.distanceM <= 250) return 60;
        return 10;
    }
    return 30;
}

bool speedDropTieComesBefore(const AlertState &left, const AlertState &right) {
    return left.kind == AlertKind::SpeedDrop && right.kind == AlertKind::SpeedDrop &&
           left.distanceM == right.distanceM && left.valueKmh > right.valueKmh;
}

bool alertPriorityComesBefore(const AlertState &left, const AlertState &right) {
    const int scoreLeft = alertPriorityScore(left);
    const int scoreRight = alertPriorityScore(right);
    if (scoreLeft != scoreRight) {
        return scoreLeft > scoreRight;
    }
    if (left.distanceM >= 0 && right.distanceM >= 0 && left.distanceM != right.distanceM) {
        return left.distanceM < right.distanceM;
    }
    return speedDropTieComesBefore(left, right);
}
}  // namespace

void HlpDecoder::resetSession() {
    session_ = 0;
    lastTimestamp_ = 0;
    haveTimestamp_ = false;
    cachedLanes_ = {};
    cachedLaneCount_ = 0;
    laneHoldAccumulatedMovingMs_ = 0;
    laneLastCheckMs_ = 0;
}

bool HlpDecoder::handleHi(const cJSON *root, HudState &state) {
    const uint32_t session = unsigned32Or(root, "sess", 0);
    const cJSON *protocolItem = cJSON_GetObjectItemCaseSensitive(root, "proto");
    const int protocol = protocolItem ? integerOr(root, "proto", -1) : 1;
    if (protocol != 1) {
        ESP_LOGW(kTag, "Producer selected unsupported HLP/%d", protocol);
        return false;
    }
    bool changed = false;
    if (session != session_) {
        const bool connected = state.connected;
        state = {};
        state.connected = connected;
        session_ = session;
        haveTimestamp_ = false;
        ESP_LOGI(kTag, "HLP session %lu established", static_cast<unsigned long>(session_));
        changed = true;
    }
    state.sessionId = session_;
    const int64_t unixSeconds = integer64Or(root, "unix", 0);
    if (unixSeconds > 0) {
        state.clockUnixSeconds = unixSeconds;
        state.timezoneOffsetMinutes = std::clamp(integerOr(root, "tz", 0), -840, 840);
        state.clockSyncMonotonicMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
        changed = true;
    }
    return changed;
}

bool HlpDecoder::decodeState(const cJSON *root, HudState &state) {
    const uint32_t timestamp = unsigned32Or(root, "ts", 0);
    if (haveTimestamp_ && static_cast<int32_t>(timestamp - lastTimestamp_) <= 0) {
        ESP_LOGD(kTag, "Discarding stale state ts=%lu last=%lu",
                 static_cast<unsigned long>(timestamp), static_cast<unsigned long>(lastTimestamp_));
        return false;
    }

    HudState decoded{};
    decoded.connected = true;
    decoded.hasProducerState = true;
    decoded.sessionId = session_;
    decoded.clockUnixSeconds = state.clockUnixSeconds;
    decoded.timezoneOffsetMinutes = state.timezoneOffsetMinutes;
    decoded.clockSyncMonotonicMs = state.clockSyncMonotonicMs;
    decoded.navigationActive = integerOr(root, "nav", 0) != 0;
    decoded.speedKmh = std::clamp(integerOr(root, "spd", 0), 0, 999);
    decoded.speedLimitKmh = std::max(0, integerOr(root, "lim", 0));
    decoded.overSpeed = integerOr(root, "over", 0) != 0;
    const int maneuverCode = integerOr(root, "trn", 0);
    const int secondManeuverCode = integerOr(root, "trn2", 0);
    decoded.maneuver = maneuverValue(maneuverCode);
    decoded.secondManeuver = maneuverValue(secondManeuverCode);
    decoded.maneuverDistanceM = integerOr(root, "dst", -1);
    decoded.roundaboutExit = std::max(0, integerOr(root, "exit", 0));
    copyUtf8(root, "st", decoded.currentStreet);
    copyUtf8(root, "st2", decoded.nextStreet);
    shortenContinueStraight(decoded.currentStreet);
    shortenContinueStraight(decoded.nextStreet);
    copyUtf8(root, "eta", decoded.eta);
    decoded.remainingMinutes = std::max(0, integerOr(root, "rmin", 0));
    const int exactRemainingMeters = integerOr(root, "rm", -1);
    const float legacyRemainingKm = std::max(0.0F, numberOr(root, "rkm", 0.0F));
    decoded.remainingMeters = exactRemainingMeters >= 0
        ? exactRemainingMeters : static_cast<int>(std::lround(legacyRemainingKm * 1000.0F));
    decoded.remainingKm = exactRemainingMeters >= 0
        ? static_cast<float>(exactRemainingMeters) / 1000.0F : legacyRemainingKm;
    decoded.noPassingZone = integerOr(root, "avg", 0) != 0;
    decoded.noPassingRemainingM = std::max(0, integerOr(root, "avgL", 0));
    decoded.noPassingRecommendedKmh = std::max(0, integerOr(root, "avgR", 0));
    decoded.noPassingProgressPct = std::clamp(integerOr(root, "avgP", 0), 0, 100);
    decoded.nearestAlert.kind = alertValue(integerOr(root, "alr", 0));
    decoded.nearestAlert.distanceM = integerOr(root, "alrD", -1);
    decoded.nearestAlert.valueKmh = std::max(0, integerOr(root, "alrV", 0));
    decodeTrafficDetails(root, "alrS", "alrM", decoded.nearestAlert);

    const cJSON *alerts = cJSON_GetObjectItemCaseSensitive(root, "alrs");
    if (cJSON_IsArray(alerts)) {
        std::array<AlertState, kMaxAlerts> parsedAlerts{};
        uint8_t parsedAlertCount = 0;
        const cJSON *entry = nullptr;
        cJSON_ArrayForEach(entry, alerts) {
            if (parsedAlertCount >= kMaxAlerts || !cJSON_IsObject(entry)) break;
            AlertState alert;
            alert.kind = alertValue(integerOr(entry, "k", 0));
            alert.distanceM = integerOr(entry, "d", -1);
            alert.valueKmh = std::max(0, integerOr(entry, "v", 0));
            decodeTrafficDetails(entry, "s", "m", alert);
            if (alert.kind == AlertKind::None) continue;
            bool duplicate = false;
            for (uint8_t index = 0; index < parsedAlertCount && !duplicate; ++index)
                duplicate = alert == parsedAlerts[index];
            if (!duplicate) parsedAlerts[parsedAlertCount++] = alert;
        }

        // The producer already orders alrs near-to-far. Preserve that order,
        // but for speed-limit drops at the exact same position prefer the
        // higher limit first. Insertion sorting only this tie condition is
        // stable, bounded, and allocation-free.
        for (uint8_t index = 1; index < parsedAlertCount; ++index) {
            const AlertState candidate = parsedAlerts[index];
            uint8_t position = index;
            while (position > 0 && alertPriorityComesBefore(candidate, parsedAlerts[position - 1])) {
                parsedAlerts[position] = parsedAlerts[position - 1];
                --position;
            }
            parsedAlerts[position] = candidate;
        }

        if (parsedAlertCount > 0) {
            // alr/alrD/alrV is only the compatibility mirror of alrs[0]. Once
            // the full opt-in list is present, derive both the dominant and
            // upcoming normalized state from that single canonical ordering.
            decoded.nearestAlert = parsedAlerts[0];
            for (uint8_t index = 1; index < parsedAlertCount; ++index)
                decoded.upcomingAlerts[decoded.upcomingAlertCount++] = parsedAlerts[index];
        }
    }

    uint8_t parsedLaneCount = 0;
    std::array<LaneState, kMaxLanes> parsedLanes{};
    const cJSON *lanes = cJSON_GetObjectItemCaseSensitive(root, "lan");
    if (cJSON_IsArray(lanes)) {
        const cJSON *lane = nullptr;
        cJSON_ArrayForEach(lane, lanes) {
            if (parsedLaneCount >= kMaxLanes) break;
            if (!cJSON_IsArray(lane) || cJSON_GetArraySize(lane) != 2) continue;
            const cJSON *directionsItem = cJSON_GetArrayItem(lane, 0);
            const cJSON *selectedItem = cJSON_GetArrayItem(lane, 1);
            if (!cJSON_IsNumber(directionsItem) || !cJSON_IsNumber(selectedItem)
                    || !std::isfinite(directionsItem->valuedouble)
                    || !std::isfinite(selectedItem->valuedouble)
                    || std::floor(directionsItem->valuedouble) != directionsItem->valuedouble
                    || std::floor(selectedItem->valuedouble) != selectedItem->valuedouble
                    || directionsItem->valuedouble <= 0.0 || directionsItem->valuedouble > 255.0
                    || selectedItem->valuedouble < 0.0 || selectedItem->valuedouble > 255.0) continue;
            const int directions = static_cast<int>(directionsItem->valuedouble);
            const int selected = static_cast<int>(selectedItem->valuedouble);
            if ((selected & ~directions) != 0) continue;
            LaneState &target = parsedLanes[parsedLaneCount++];
            target.directionMask = static_cast<uint8_t>(directions);
            target.selectedMask = static_cast<uint8_t>(selected);
        }
    }

    const int64_t nowMs = static_cast<int64_t>(esp_timer_get_time() / 1000);
    if (parsedLaneCount > 0) {
        // Ưu tiên cao nhất: Khi Waze gửi bộ lane mới (kể cả ở nhánh rẽ sát sau), ghi đè và nhảy liền lập tức
        cachedLanes_ = parsedLanes;
        cachedLaneCount_ = parsedLaneCount;
        laneHoldAccumulatedMovingMs_ = 0;
        laneLastCheckMs_ = nowMs;
        decoded.lanes = cachedLanes_;
        decoded.laneCount = cachedLaneCount_;
    } else if (cachedLaneCount_ > 0 && decoded.navigationActive) {
        // Khi không có lane mới từ Waze:
        const int64_t dt = laneLastCheckMs_ > 0 ? std::clamp(nowMs - laneLastCheckMs_, 0LL, 1000LL) : 0LL;
        laneLastCheckMs_ = nowMs;

        if (decoded.speedKmh >= 25) {
            // Khi tốc độ lên cao (>= 25 km/h): Bắt đầu/tiếp tục đếm tích lũy thời gian
            laneHoldAccumulatedMovingMs_ += dt;
        }
        // Khi tốc độ < 25 km/h (dừng đèn đỏ, kẹt xe): TẠM NGƯNG ĐẾM GIÂY (laneHoldAccumulatedMovingMs_ giữ nguyên)

        constexpr int64_t kLaneHoldTimeoutMs = 15000;
        if (laneHoldAccumulatedMovingMs_ < kLaneHoldTimeoutMs) {
            decoded.lanes = cachedLanes_;
            decoded.laneCount = cachedLaneCount_;
        } else {
            // Đã chạy trên 25 km/h đủ 15 giây mà không có lane mới: Xóa lane cũ
            cachedLaneCount_ = 0;
            laneHoldAccumulatedMovingMs_ = 0;
            decoded.laneCount = 0;
        }
    } else {
        cachedLaneCount_ = 0;
        laneHoldAccumulatedMovingMs_ = 0;
        laneLastCheckMs_ = 0;
        decoded.laneCount = 0;
    }

    // Minimum-speed state has no HLP/1 field yet. Only the compile-time mock may populate it.
    decoded.hasMinimumSpeed = false;
    decoded.producerTimestamp = timestamp;
    state = decoded;
    lastTimestamp_ = timestamp;
    haveTimestamp_ = true;
    return true;
}

}  // namespace waze_hud
