#include "display/hud_renderer.h"

#include "display/colors.h"
#include "display/display_driver.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace waze_hud {
namespace {
constexpr char kTag[] = "DISPLAY";

constexpr int mainY(int value) {
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    return value;
#else
    return value * layout::UiYScaleNumerator / layout::UiYScaleDenominator;
#endif
}

constexpr int screenY(int value) {
    return value * layout::UiYScaleNumerator / layout::UiYScaleDenominator;
}

uint16_t foreground(const DeviceSettings &settings) {
    return settings.theme == UiTheme::Night ? colors::rgb565(220, 105, 80) : colors::Foreground;
}

template <typename Left, typename Right>
bool sameText(const Left &left, const Right &right) { return std::strcmp(left.data(), right.data()) == 0; }

bool maneuverChanged(const HudState &a, const HudState &b) {
    return a.maneuver != b.maneuver || a.secondManeuver != b.secondManeuver ||
           a.maneuverDistanceM != b.maneuverDistanceM ||
           a.roundaboutExit != b.roundaboutExit ||
           !sameText(a.nextStreet, b.nextStreet);
}

bool isRoundaboutManeuver(Maneuver maneuver) {
    return maneuver == Maneuver::Roundabout || maneuver == Maneuver::RoundaboutLeft ||
           maneuver == Maneuver::RoundaboutRight ||
           maneuver == Maneuver::RoundaboutStraight ||
           maneuver == Maneuver::RoundaboutUTurn;
}

bool alertsChanged(const HudState &a, const HudState &b) {
    if (!(a.nearestAlert == b.nearestAlert) || a.upcomingAlertCount != b.upcomingAlertCount ||
        a.noPassingZone != b.noPassingZone || a.noPassingRemainingM != b.noPassingRemainingM) return true;
    for (uint8_t i = 0; i < a.upcomingAlertCount; ++i)
        if (!(a.upcomingAlerts[i] == b.upcomingAlerts[i])) return true;
    return false;
}

bool guidanceChanged(const HudState &a, const HudState &b) {
    if (!sameText(a.eta, b.eta) || a.laneCount != b.laneCount ||
        a.remainingMinutes != b.remainingMinutes ||
        static_cast<int>(a.remainingKm * 10) != static_cast<int>(b.remainingKm * 10) ||
        alertsChanged(a, b))
        return true;
    for (uint8_t index = 0; index < a.laneCount; ++index)
        if (!(a.lanes[index] == b.lanes[index])) return true;
    return false;
}

bool hasSettingsChanged(const DeviceSettings &a, const DeviceSettings &b) {
    return a.fixedBrightness != b.fixedBrightness || a.brightness != b.brightness ||
           a.theme != b.theme || a.invertColor != b.invertColor ||
           a.mirrorHud != b.mirrorHud || a.rotateDisplay != b.rotateDisplay ||
           a.overspeedOffsetKmh != b.overspeedOffsetKmh ||
           a.offsetX != b.offsetX || a.offsetY != b.offsetY || a.revision != b.revision;
}

bool firmwareOverspeed(const HudState &state, const DeviceSettings &settings) {
    if (state.speedLimitKmh <= 0) return false;
    const int threshold = std::max(0, state.speedLimitKmh +
                                     static_cast<int>(settings.overspeedOffsetKmh));
    return state.speedKmh > threshold;
}

uint16_t alertDistanceColor(int distanceM, uint16_t normalColor) {
    if (distanceM < 0) return normalColor;
    if (distanceM < 100) return colors::Red;
    if (distanceM <= 300) return colors::Amber;
    if (distanceM <= 600) return colors::White;
    return colors::Green;
}

#if __has_include("serial/serial_transport.h")
uint16_t transportColor(const SystemStatusSnapshot &status) {
    return status.transportConnected ? colors::Green : colors::Muted;
}
#else
uint16_t bleSignalColor(const SystemStatusSnapshot &status) {
    if (!status.bleConnected) return colors::Muted;
    if (status.bleRssiDbm >= -60) return colors::Green;
    if (status.bleRssiDbm >= -75) return colors::Blue;
    if (status.bleRssiDbm >= -85) return colors::Amber;
    return colors::Red;
}
#endif

int64_t localClockMillis(const HudState &state) {
    if (state.clockUnixSeconds <= 0) return INT64_MIN;
    const uint64_t nowMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    const uint64_t elapsedMs = nowMs >= state.clockSyncMonotonicMs
        ? nowMs - state.clockSyncMonotonicMs : 0U;
    return state.clockUnixSeconds * 1000LL + static_cast<int64_t>(elapsedMs) +
           static_cast<int64_t>(state.timezoneOffsetMinutes) * 60000LL;
}

uint8_t calculateTimeOfDayBrightness(int normalizedMinute, uint8_t maxBrightness) {
    constexpr int kDayStartMinute = 6 * 60;          // 06:00 (360)
    constexpr int kSunsetStartMinute = 17 * 60 + 30; // 17:30 (1050)
    constexpr int kNightStartMinute = 19 * 60;        // 19:00 (1140)
    constexpr int kSunriseStartMinute = 5 * 60;       // 05:00 (300)
    constexpr uint8_t kNightBrightness = 20;

    if (maxBrightness < kNightBrightness) maxBrightness = 100;

    if (normalizedMinute >= kDayStartMinute && normalizedMinute < kSunsetStartMinute) {
        return maxBrightness;
    } else if (normalizedMinute >= kSunsetStartMinute && normalizedMinute < kNightStartMinute) {
        // Sunset transition: 17:30 -> 19:00 (Max -> 20%)
        const float progress = static_cast<float>(normalizedMinute - kSunsetStartMinute) /
                               static_cast<float>(kNightStartMinute - kSunsetStartMinute);
        return static_cast<uint8_t>(maxBrightness - progress * (maxBrightness - kNightBrightness) + 0.5f);
    } else if (normalizedMinute >= kNightStartMinute || normalizedMinute < kSunriseStartMinute) {
        // Night: 19:00 -> 05:00 (20%)
        return kNightBrightness;
    } else {
        // Sunrise transition: 05:00 -> 06:00 (20% -> Max)
        const float progress = static_cast<float>(normalizedMinute - kSunriseStartMinute) /
                               static_cast<float>(kDayStartMinute - kSunriseStartMinute);
        return static_cast<uint8_t>(kNightBrightness + progress * (maxBrightness - kNightBrightness) + 0.5f);
    }
}

const char *displayStreet(const HudState &state) {
    // Hiển thị tên đường
    if (state.currentStreet[0] != 0) return state.currentStreet.data();
    return "Cầu đường chưa đặt tên";
}

bool sameRegion(const Rect &left, const Rect &right) {
    return left.x == right.x && left.y == right.y && left.width == right.width &&
           left.height == right.height;
}

void arrowHead(Canvas &canvas, int x, int y, int dx, int dy, uint16_t color, int thickness) {
    if (std::abs(dx) >= std::abs(dy)) {
        const int sign = dx >= 0 ? 1 : -1;
        canvas.line(x, y, x - sign * 9, y - 7, color, thickness);
        canvas.line(x, y, x - sign * 9, y + 7, color, thickness);
    } else {
        const int sign = dy >= 0 ? 1 : -1;
        canvas.line(x, y, x - 7, y - sign * 9, color, thickness);
        canvas.line(x, y, x + 7, y - sign * 9, color, thickness);
    }
}

void drawArrowHeadDir(Canvas &canvas, int tipX, int tipY, int dir, uint16_t color, int size = 8) {
    const int w = size * 3 / 4;
    if (dir == 0) { // UP
        for (int i = 0; i <= size; ++i) {
            const int span = (w * i) / size;
            canvas.fillRect(tipX - span, tipY + i, span * 2 + 1, 1, color);
        }
    } else if (dir == 1) { // DOWN
        for (int i = 0; i <= size; ++i) {
            const int span = (w * i) / size;
            canvas.fillRect(tipX - span, tipY - i, span * 2 + 1, 1, color);
        }
    } else if (dir == 2) { // LEFT
        for (int i = 0; i <= size; ++i) {
            const int span = (w * i) / size;
            canvas.fillRect(tipX + i, tipY - span, 1, span * 2 + 1, color);
        }
    } else if (dir == 3) { // RIGHT
        for (int i = 0; i <= size; ++i) {
            const int span = (w * i) / size;
            canvas.fillRect(tipX - i, tipY - span, 1, span * 2 + 1, color);
        }
    }
}

static void drawFillTriangle(Canvas &canvas, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }

    auto edgeX = [](int ya, int yb, int xa, int xb, int y) -> int {
        if (ya == yb) return xa;
        return xa + (xb - xa) * (y - ya) / (yb - ya);
    };

    for (int y = y0; y <= y2; ++y) {
        int xl = (y < y1) ? edgeX(y0, y1, x0, x1, y) : edgeX(y1, y2, x1, x2, y);
        int xr = edgeX(y0, y2, x0, x2, y);
        if (xl > xr) std::swap(xl, xr);
        canvas.fillRect(xl, y, xr - xl + 1, 1, color);
    }
}

struct ArrowBase { int x; int y; };

static ArrowBase drawArrow45(Canvas &canvas, int tipX, int tipY, bool isRight, uint16_t color,
                             int length = 8, int width = 12, float notch = 2.0f) {
    const float angleDeg = isRight ? 48.0f : -48.0f;
    constexpr float kDegToRad = 3.14159265f / 180.0f;
    const float rad = angleDeg * kDegToRad;
    const float ux = std::sin(rad);
    const float uy = -std::cos(rad);
    const float vx = std::cos(rad);
    const float vy = std::sin(rad);

    const float bx = static_cast<float>(tipX) - static_cast<float>(length) * ux;
    const float by = static_cast<float>(tipY) - static_cast<float>(length) * uy;

    const float nx = bx + notch * ux;
    const float ny = by + notch * uy;

    const float w2 = static_cast<float>(width) / 2.0f;
    const int wLx = static_cast<int>(std::round(bx - w2 * vx));
    const int wLy = static_cast<int>(std::round(by - w2 * vy));
    const int wRx = static_cast<int>(std::round(bx + w2 * vx));
    const int wRy = static_cast<int>(std::round(by + w2 * vy));

    const int notchX = static_cast<int>(std::round(nx));
    const int notchY = static_cast<int>(std::round(ny));

    drawFillTriangle(canvas, tipX, tipY, wLx, wLy, notchX, notchY, color);
    drawFillTriangle(canvas, tipX, tipY, wRx, wRy, notchX, notchY, color);

    return {static_cast<int>(std::round(bx)), static_cast<int>(std::round(by))};
}

static void drawQuadCurve(Canvas &canvas, int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color, int thickness = 4) {
    constexpr int kSteps = 8;
    int px = x0;
    int py = y0;
    for (int i = 1; i <= kSteps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSteps);
        const float it = 1.0f - t;
        const int xt = static_cast<int>(std::round(it * it * x0 + 2.0f * it * t * x1 + t * t * x2));
        const int yt = static_cast<int>(std::round(it * it * y0 + 2.0f * it * t * y1 + t * t * y2));
        canvas.line(px, py, xt, yt, color, thickness);
        px = xt;
        py = yt;
    }
}

void drawGuidanceLane(Canvas &canvas, int x, int spacing, const LaneState &lane, uint16_t foregroundColor) {
    constexpr int baseY = 66;
    constexpr int midY = 38;
    constexpr int topY = 14;
    constexpr int stroke = 4;
    constexpr int arrowSize = 8; // Reduced to prevent overlap

    // Strictly constrain the width of the arrows to prevent eating into adjacent lanes
    const int leftBound = x - 15;
    const int rightBound = x + 15;

    const bool hasStraight = (lane.directionMask & 0x01) != 0;
    const bool hasSlightLeft = (lane.directionMask & 0x02) != 0;
    const bool hasLeft = (lane.directionMask & 0x04) != 0;
    const bool hasSlightRight = (lane.directionMask & 0x10) != 0;
    const bool hasRight = (lane.directionMask & 0x20) != 0;
    const bool hasUTurn = (lane.directionMask & 0x80) != 0;

    const bool straightActive = (lane.selectedMask & 0x01) != 0;
    const bool slightLeftActive = (lane.selectedMask & 0x02) != 0;
    const bool leftActive = (lane.selectedMask & 0x04) != 0;
    const bool slightRightActive = (lane.selectedMask & 0x10) != 0;
    const bool rightActive = (lane.selectedMask & 0x20) != 0;
    const bool uTurnActive = (lane.selectedMask & 0x80) != 0;

    const bool isLaneSelected = lane.selectedMask != 0;

    const uint16_t cStraight = straightActive ? foregroundColor : colors::Muted;
    const uint16_t cLeft = leftActive ? foregroundColor : colors::Muted;
    const uint16_t cSlightLeft = slightLeftActive ? foregroundColor : colors::Muted;
    const uint16_t cRight = rightActive ? foregroundColor : colors::Muted;
    const uint16_t cSlightRight = slightRightActive ? foregroundColor : colors::Muted;
    const uint16_t cUTurn = uTurnActive ? foregroundColor : colors::Muted;
    const uint16_t baseCol = isLaneSelected ? foregroundColor : colors::Muted;

    if (isLaneSelected) {
        const int barW = std::clamp(spacing - 6, 10, 32);
        canvas.fillRect(x - barW / 2, 73, barW, 4, colors::Green);
    }

    if (hasStraight && hasUTurn) {
        const int shiftX = hasLeft ? 4 : 0;
        const int sx = x + 5 + shiftX;
        const int lx = x - 5 + shiftX;
        const int forkY = midY - 2;

        for (const bool passActive : {false, true}) {
            if (uTurnActive == passActive) {
                canvas.line(sx, forkY + 3, sx - 3, forkY, cUTurn, stroke);
                canvas.line(sx - 3, forkY, lx + 3, forkY, cUTurn, stroke);
                canvas.line(lx + 3, forkY, lx, forkY + 3, cUTurn, stroke);
                canvas.line(lx, forkY + 3, lx, midY + 4, cUTurn, stroke);
                canvas.fillCircle(sx, forkY + 3, stroke / 2, cUTurn);
                canvas.fillCircle(sx - 3, forkY, stroke / 2, cUTurn);
                canvas.fillCircle(lx + 3, forkY, stroke / 2, cUTurn);
                canvas.fillCircle(lx, forkY + 3, stroke / 2, cUTurn);
                drawArrowHeadDir(canvas, lx, midY + 4 + arrowSize, 1, cUTurn, arrowSize);
            }
            if (hasLeft && (leftActive == passActive)) {
                const int lTip = lx - 14;
                const int lBase = lTip + arrowSize;
                canvas.line(lx, forkY, lBase, forkY, cLeft, stroke);
                drawArrowHeadDir(canvas, lTip, forkY, 2, cLeft, arrowSize);
            }
            if (hasRight && (rightActive == passActive)) {
                const int rBase = rightBound - arrowSize;
                canvas.line(sx, midY, rBase, midY, cRight, stroke);
                drawArrowHeadDir(canvas, rightBound, midY, 3, cRight, arrowSize);
            }
            if (straightActive == passActive) {
                canvas.line(sx, forkY, sx, topY, cStraight, stroke);
                drawArrowHeadDir(canvas, sx, topY - arrowSize / 2, 0, cStraight, arrowSize);
            }
        }
        canvas.line(sx, baseY, sx, forkY, baseCol, stroke);
        return;
    }

    if (hasUTurn && !hasStraight) {
        const int shiftX = hasLeft ? 6 : 0;
        const int rx = x + 6 + shiftX;
        const int lx = x - 6 + shiftX;

        for (const bool passActive : {false, true}) {
            if (uTurnActive == passActive) {
                canvas.line(rx, baseY, rx, topY + 4 + 3, cUTurn, stroke);
                canvas.line(rx, topY + 4 + 3, rx - 3, topY + 4, cUTurn, stroke);
                canvas.line(rx - 3, topY + 4, lx + 3, topY + 4, cUTurn, stroke);
                canvas.line(lx + 3, topY + 4, lx, topY + 4 + 3, cUTurn, stroke);
                canvas.line(lx, topY + 4 + 3, lx, midY - 4, cUTurn, stroke);
                canvas.fillCircle(rx, topY + 4 + 3, stroke / 2, cUTurn);
                canvas.fillCircle(rx - 3, topY + 4, stroke / 2, cUTurn);
                canvas.fillCircle(lx + 3, topY + 4, stroke / 2, cUTurn);
                canvas.fillCircle(lx, topY + 4 + 3, stroke / 2, cUTurn);
                drawArrowHeadDir(canvas, lx, midY - 4 + arrowSize, 1, cUTurn, arrowSize);
            }
            if (hasLeft && (leftActive == passActive)) {
                const int lTip = lx - 15;
                const int lBase = lTip + arrowSize;
                canvas.line(lx, topY + 4, lBase, topY + 4, cLeft, stroke);
                drawArrowHeadDir(canvas, lTip, topY + 4, 2, cLeft, arrowSize);
            }
            if (hasRight && (rightActive == passActive)) {
                const int rBase = rightBound - arrowSize;
                canvas.line(rx, midY, rBase, midY, cRight, stroke);
                drawArrowHeadDir(canvas, rightBound, midY, 3, cRight, arrowSize);
            }
        }
        return;
    }

    if (hasLeft && !hasStraight && !hasRight) {
        if (hasSlightLeft) {
            const int sx = x + 4;
            const int lBase = leftBound + arrowSize;
            for (const bool passActive : {false, true}) {
                if (leftActive == passActive) {
                    canvas.line(sx, midY + 8, lBase, midY + 8, cLeft, stroke);
                    drawArrowHeadDir(canvas, leftBound, midY + 8, 2, cLeft, arrowSize);
                }
                if (slightLeftActive == passActive) {
                    const ArrowBase base = drawArrow45(canvas, x - 8, topY + 2, false, cSlightLeft);
                    drawQuadCurve(canvas, sx, midY + 8, sx, midY - 6, base.x, base.y, cSlightLeft, stroke);
                }
            }
            canvas.line(sx, baseY, sx, midY + 8, baseCol, stroke);
            return;
        }
        const int rx = x + 8;
        const int lBase = leftBound + arrowSize;
        canvas.line(rx, baseY, rx, topY + 4 + 3, cLeft, stroke);
        canvas.line(rx, topY + 4 + 3, rx - 3, topY + 4, cLeft, stroke);
        canvas.line(rx - 3, topY + 4, lBase, topY + 4, cLeft, stroke);
        canvas.fillCircle(rx, topY + 4 + 3, stroke / 2, cLeft);
        canvas.fillCircle(rx - 3, topY + 4, stroke / 2, cLeft);
        drawArrowHeadDir(canvas, leftBound, topY + 4, 2, cLeft, arrowSize);
        return;
    }

    if (hasRight && !hasStraight && !hasLeft) {
        if (hasSlightRight) {
            const int sx = x - 4;
            const int rBase = rightBound - arrowSize;
            for (const bool passActive : {false, true}) {
                if (rightActive == passActive) {
                    canvas.line(sx, midY + 8, rBase, midY + 8, cRight, stroke);
                    drawArrowHeadDir(canvas, rightBound, midY + 8, 3, cRight, arrowSize);
                }
                if (slightRightActive == passActive) {
                    const ArrowBase base = drawArrow45(canvas, x + 8, topY + 2, true, cSlightRight);
                    drawQuadCurve(canvas, sx, midY + 8, sx, midY - 6, base.x, base.y, cSlightRight, stroke);
                }
            }
            canvas.line(sx, baseY, sx, midY + 8, baseCol, stroke);
            return;
        }
        const int lx = x - 8;
        const int rBase = rightBound - arrowSize;
        canvas.line(lx, baseY, lx, topY + 4 + 3, cRight, stroke);
        canvas.line(lx, topY + 4 + 3, lx + 3, topY + 4, cRight, stroke);
        canvas.line(lx + 3, topY + 4, rBase, topY + 4, cRight, stroke);
        canvas.fillCircle(lx, topY + 4 + 3, stroke / 2, cRight);
        canvas.fillCircle(lx + 3, topY + 4, stroke / 2, cRight);
        drawArrowHeadDir(canvas, rightBound, topY + 4, 3, cRight, arrowSize);
        return;
    }

    if (hasStraight) {
        for (const bool passActive : {false, true}) {
            if (straightActive == passActive) {
                canvas.line(x, midY, x, topY, cStraight, stroke);
                drawArrowHeadDir(canvas, x, topY - arrowSize / 2, 0, cStraight, arrowSize);
            }

            if (hasLeft && (leftActive == passActive)) {
                const int lBase = leftBound + arrowSize;
                canvas.line(x, midY + 3, x - 3, midY, cLeft, stroke);
                canvas.line(x - 3, midY, lBase, midY, cLeft, stroke);
                canvas.fillCircle(x, midY + 3, stroke / 2, cLeft);
                canvas.fillCircle(x - 3, midY, stroke / 2, cLeft);
                drawArrowHeadDir(canvas, leftBound, midY, 2, cLeft, arrowSize);
            } else if (hasSlightLeft && (slightLeftActive == passActive)) {
                const ArrowBase base = drawArrow45(canvas, x - 13, topY + 8, false, cSlightLeft);
                drawQuadCurve(canvas, x, midY + 18, x - 3, midY, base.x, base.y, cSlightLeft, stroke);
            }

            if (hasRight && (rightActive == passActive)) {
                const int rBase = rightBound - arrowSize;
                canvas.line(x, midY + 3, x + 3, midY, cRight, stroke);
                canvas.line(x + 3, midY, rBase, midY, cRight, stroke);
                canvas.fillCircle(x, midY + 3, stroke / 2, cRight);
                canvas.fillCircle(x + 3, midY, stroke / 2, cRight);
                drawArrowHeadDir(canvas, rightBound, midY, 3, cRight, arrowSize);
            } else if (hasSlightRight && (slightRightActive == passActive)) {
                const ArrowBase base = drawArrow45(canvas, x + 13, topY + 8, true, cSlightRight);
                drawQuadCurve(canvas, x, midY + 18, x + 3, midY, base.x, base.y, cSlightRight, stroke);
            }
        }
        canvas.line(x, baseY, x, midY, baseCol, stroke);
        return;
    }

    if (hasSlightLeft && !hasSlightRight && !hasRight) {
        const ArrowBase base = drawArrow45(canvas, x - 9, topY + 2, false, cSlightLeft);
        canvas.line(x + 5, baseY, x + 5, midY + 10, cSlightLeft, stroke);
        drawQuadCurve(canvas, x + 5, midY + 10, x + 4, midY - 2, base.x, base.y, cSlightLeft, stroke);
        return;
    }

    if (hasSlightRight && !hasSlightLeft && !hasLeft) {
        const ArrowBase base = drawArrow45(canvas, x + 9, topY + 2, true, cSlightRight);
        canvas.line(x - 5, baseY, x - 5, midY + 10, cSlightRight, stroke);
        drawQuadCurve(canvas, x - 5, midY + 10, x - 4, midY - 2, base.x, base.y, cSlightRight, stroke);
        return;
    }

    if (hasSlightLeft && hasSlightRight) {
        for (const bool passActive : {false, true}) {
            if (slightLeftActive == passActive) {
                const ArrowBase baseL = drawArrow45(canvas, x - 13, topY + 4, false, cSlightLeft);
                drawQuadCurve(canvas, x, midY + 10, x - 4, midY - 4, baseL.x, baseL.y, cSlightLeft, stroke);
            }
            if (slightRightActive == passActive) {
                const ArrowBase baseR = drawArrow45(canvas, x + 13, topY + 4, true, cSlightRight);
                drawQuadCurve(canvas, x, midY + 10, x + 4, midY - 4, baseR.x, baseR.y, cSlightRight, stroke);
            }
        }
        canvas.line(x, baseY, x, midY + 10, baseCol, stroke);
        return;
    }

    canvas.line(x, baseY, x, topY, baseCol, stroke);
    drawArrowHeadDir(canvas, x, topY - arrowSize / 2, 0, baseCol, arrowSize);
}



const assets::AlphaMask *maneuverAsset(Maneuver maneuver) {
    switch (maneuver) {
        case Maneuver::Continue: return &assets::kManeuverContinue;
        case Maneuver::Left: return &assets::kManeuverLeft;
        case Maneuver::Right: return &assets::kManeuverRight;
        case Maneuver::UTurn: return &assets::kManeuverUTurn;
        case Maneuver::Roundabout: return &assets::kManeuverRoundabout;
        case Maneuver::RoundaboutLeft: return &assets::kManeuverRoundaboutLeft;
        case Maneuver::RoundaboutRight: return &assets::kManeuverRoundaboutRight;
        case Maneuver::RoundaboutStraight: return &assets::kManeuverRoundaboutStraight;
        case Maneuver::RoundaboutUTurn: return &assets::kManeuverRoundaboutUTurn;
        case Maneuver::KeepLeft: return &assets::kManeuverExitLeft;
        case Maneuver::KeepRight: return &assets::kManeuverExitRight;
        case Maneuver::ExitLeft: return &assets::kManeuverExitLeft;
        case Maneuver::ExitRight: return &assets::kManeuverExitRight;
        case Maneuver::Arrive: return &assets::kManeuverArrive;
        default: return nullptr;
    }
}

void drawRoundaboutExit(Canvas &canvas, int exit, uint16_t color) {
    if (exit <= 0) return;
    char number[12];
    std::snprintf(number,sizeof(number),"%d",exit);
    constexpr int centerX = 42;
    constexpr int width = 36;
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    constexpr int centerY = 54;
#else
    const int centerY = mainY(65);
#endif
    canvas.fontText(centerX-width/2, centerY - assets::kNumberMedium.lineHeight/2,
                    number, assets::kNumberMedium, color, width, true);
}

enum class SpeedSignContext { Current, AlertLarge, AlertSmall };

const assets::ColorBitmap *speedLimitAsset(int value, SpeedSignContext context) {
    for (std::size_t index = 0; index < assets::kSpeedLimitAssetCount; ++index) {
        const assets::SpeedLimitAssetSet &entry = assets::kSpeedLimitAssets[index];
        if (entry.value != value) continue;
        switch (context) {
            case SpeedSignContext::Current: return entry.current;
            case SpeedSignContext::AlertLarge: return entry.alertLarge;
            case SpeedSignContext::AlertSmall: return entry.alertSmall;
        }
    }
    return nullptr;
}

void drawManeuverIcon(Canvas &canvas, Maneuver maneuver, int exit, uint16_t color) {
    constexpr int cx = 42;
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    constexpr int maneuverAssetY = 34;
    constexpr int top = 38;
    constexpr int bottom = 90;
    constexpr int roundaboutCenterY = 62;
    constexpr int junctionY = 67;
#else
    const int maneuverAssetY = mainY(34);
    const int top = mainY(38);
    const int bottom = mainY(92);
    const int roundaboutCenterY = mainY(65);
    const int junctionY = mainY(69);
#endif
    const int thick = 5;
    if (maneuver == Maneuver::None) return;
    if (const assets::AlphaMask *asset = maneuverAsset(maneuver)) {
        canvas.alphaMask(12, maneuverAssetY, *asset, color);
        if (isRoundaboutManeuver(maneuver) && exit > 0) {
            drawRoundaboutExit(canvas,exit,color);
        }
        return;
    }
    if (maneuver == Maneuver::Arrive) {
        canvas.line(cx - 18, top + 7, cx - 18, bottom - 3, color, 3);
        canvas.fillRect(cx - 15, top + 8, 28, 18, color);
        canvas.fillRect(cx - 10, top + 13, 5, 5, colors::Background);
        canvas.fillRect(cx, top + 13, 5, 5, colors::Background);
        return;
    }
    if (isRoundaboutManeuver(maneuver)) {
        canvas.circle(cx, roundaboutCenterY, 20, color, 4);
        canvas.line(cx, bottom, cx, roundaboutCenterY + 18, color, thick);
        if (maneuver == Maneuver::RoundaboutStraight) {
            canvas.line(cx, roundaboutCenterY - 20, cx, top, color, thick);
            arrowHead(canvas, cx, top, 0, -1, color, 3);
            drawRoundaboutExit(canvas,exit,color);
            return;
        }
        if (maneuver == Maneuver::RoundaboutUTurn) {
            const int endX = cx - 27;
            canvas.line(cx - 19, roundaboutCenterY, endX, roundaboutCenterY, color, thick);
            canvas.line(endX, roundaboutCenterY, endX, roundaboutCenterY + 13, color, thick);
            arrowHead(canvas, endX, roundaboutCenterY + 13, 0, 1, color, 3);
            drawRoundaboutExit(canvas,exit,color);
            return;
        }
        const bool left = maneuver == Maneuver::RoundaboutLeft;
        const int endX = left ? cx - 27 : cx + 27;
        canvas.line(left ? cx - 19 : cx + 19, roundaboutCenterY, endX, roundaboutCenterY, color, thick);
        arrowHead(canvas, endX, roundaboutCenterY, left ? -1 : 1, 0, color, 3);
        drawRoundaboutExit(canvas,exit,color);
        return;
    }
    if (maneuver == Maneuver::UTurn || maneuver == Maneuver::UTurnRightReserved) {
        const bool right = maneuver == Maneuver::UTurnRightReserved;
        const int side = right ? 1 : -1;
        canvas.line(cx, bottom, cx, top + 17, color, thick);
        canvas.line(cx, top + 17, cx + side * 16, top + 5, color, thick);
        canvas.line(cx + side * 16, top + 5, cx + side * 27, top + 17, color, thick);
        canvas.line(cx + side * 27, top + 17, cx + side * 27, top + 30, color, thick);
        arrowHead(canvas, cx + side * 27, top + 30, 0, 1, color, 3);
        return;
    }

    int endX = cx, endY = top;
    switch (maneuver) {
        case Maneuver::Left: endX = 15; endY = top + 15; break;
        case Maneuver::Right: endX = 69; endY = top + 15; break;
        case Maneuver::SlightLeft: case Maneuver::KeepLeft: endX = 21; endY = top + 4; break;
        case Maneuver::SlightRight: case Maneuver::KeepRight: endX = 63; endY = top + 4; break;
        case Maneuver::SharpLeft: case Maneuver::ExitLeft: endX = 14; endY = top + 35; break;
        case Maneuver::SharpRight: case Maneuver::ExitRight: endX = 70; endY = top + 35; break;
        default: break;
    }
    canvas.line(cx, bottom, cx, junctionY, color, thick);
    canvas.line(cx, junctionY, endX, endY, color, thick);
    arrowHead(canvas, endX, endY, endX - cx, endY - junctionY, color, 3);
}

const assets::ColorBitmap *alertAsset(AlertKind kind, bool dominant) {
    const uint8_t code = static_cast<uint8_t>(kind);
    for (std::size_t index = 0; index < assets::kAlertAssetCount; ++index) {
        const assets::AlertAssetSet &entry = assets::kAlertAssets[index];
        if (entry.code == code) return dominant ? entry.large : entry.small;
    }
    return nullptr;
}

uint16_t trafficSeverityColor(uint8_t severity) {
    switch (severity) {
        case 1: return colors::Green;
        case 2: return colors::Amber;
        case 3: return colors::rgb565(255, 112, 24);
        case 4: case 5: return colors::Red;
        default: return colors::Muted;
    }
}

const assets::ColorBitmap *trafficJamAsset(uint8_t severity, bool dominant) {
    switch (severity) {
        case 1: return dominant ? &assets::kAlertTrafficJam1Large
                                : &assets::kAlertTrafficJam1Small;
        case 2: return dominant ? &assets::kAlertTrafficJam2Large
                                : &assets::kAlertTrafficJam2Small;
        case 4: case 5: return dominant ? &assets::kAlertTrafficJam4Large
                                        : &assets::kAlertTrafficJam4Small;
        default: return alertAsset(AlertKind::TrafficJam, dominant);
    }
}

const char *trafficSeverityLabel(uint8_t severity) {
    switch (severity) {
        case 1: return "NHẸ";
        case 2: return "VỪA";
        case 3: return "NẶNG";
        case 4: return "ĐỨNG IM";
        case 5: return "ĐƯỜNG ĐÓNG";
        default: return "KẸT XE";
    }
}

void drawTrafficSeverityTicks(Canvas &canvas, int centerX, int y,
                              uint8_t severity, uint16_t color) {
    if (severity == 0) return;
    constexpr int tickWidth = 4;
    constexpr int gap = 2;
    constexpr int totalWidth = 5 * tickWidth + 4 * gap;
    const int startX = centerX - totalWidth / 2;
    for (int tick = 0; tick < 5; ++tick)
        canvas.fillRect(startX + tick * (tickWidth + gap), y, tickWidth, 3,
                        tick < severity ? color : colors::Muted);
}

const char *alertKindLabel(AlertKind kind) {
    switch (kind) {
        case AlertKind::Police: return "CSGT";
        case AlertKind::SpeedCamera: return "CAM TỐC ĐỘ";
        case AlertKind::RedLightCamera: return "CAM ĐÈN ĐỎ";
        case AlertKind::Hazard: return "NGUY HIỂM";
        case AlertKind::Accident: return "TAI NẠN";
        case AlertKind::TrafficJam: return "ÙN TẮC";
        case AlertKind::RoadClosed: return "CẤM ĐƯỜNG";
        case AlertKind::SpeedDrop: return "HẠ TỐC ĐỘ";
        case AlertKind::NoPassing: return "CẤM VƯỢT";
        case AlertKind::EndNoPassing: return "HẾT CẤM VƯỢT";
        case AlertKind::Railway: return "ĐƯỜNG SẮT";
        case AlertKind::TollBooth: return "TRẠM THU PHÍ";
        case AlertKind::StoppedVehicle: return "XE DỪNG";
        case AlertKind::Construction: return "CÔNG TRƯỜNG";
        case AlertKind::Pothole: return "Ổ GÀ";
        case AlertKind::Weather: return "THỜI TIẾT";
        case AlertKind::BlockedLane: return "CHẮN LÀN";
        case AlertKind::DangerousRoad: return "ĐƯỜNG NGUY HIỂM";
        case AlertKind::ExpresswayExit: return "LỐI RA CAO TỐC";
        case AlertKind::ExpresswayRestStop: return "TRẠM DỪNG CAO TỐC";
        case AlertKind::RestStop: return "TRẠM DỪNG";
        case AlertKind::EndSpeedRestriction: return "HẾT GIỚI HẠN TỐC ĐỘ";
        case AlertKind::ResidentialStart: return "KHU DÂN CƯ";
        case AlertKind::ResidentialEnd: return "HẾT KHU DÂN CƯ";
        case AlertKind::EndAllProhibitions: return "HẾT MỌI LỆNH CẤM";
        case AlertKind::NoCar: return "CẤM Ô TÔ";
        case AlertKind::NoMotorcycle: return "CẤM XE MÁY";
        case AlertKind::NoLeftTurn: return "CẤM RẼ TRÁI";
        case AlertKind::NoRightTurn: return "CẤM RẼ PHẢI";
        case AlertKind::NoUTurn: return "CẤM QUAY ĐẦU";
        case AlertKind::NoStraight: return "CẤM ĐI THẲNG";
        case AlertKind::MandatoryStraight: return "ĐI THẲNG";
        case AlertKind::MandatoryRight: return "RẼ PHẢI";
        case AlertKind::MandatoryLeft: return "RẼ TRÁI";
        case AlertKind::CarLane: return "LÀN Ô TÔ";
        case AlertKind::MotorcycleLane: return "LÀN XE MÁY";
        case AlertKind::OneWay: return "ĐƯỜNG 1 CHIỀU";
        case AlertKind::ProhibitedRoad: return "ĐƯỜNG CẤM";
        case AlertKind::CombinedTurnRestriction: return "CẤM RẼ";
        case AlertKind::PhoneCamera: return "CAM ĐIỆN THOẠI";
        case AlertKind::DummyCamera: return "CAM GIẢ";
        case AlertKind::SeatbeltCamera: return "CAM DÂY AN TOÀN";
        case AlertKind::DistanceCamera: return "CAM KHOẢNG CÁCH";
        case AlertKind::BusLaneCamera: return "CAM LÀN BUS";
        case AlertKind::NoiseCamera: return "CAM TIẾNG ỒN";
        case AlertKind::StopSignCamera: return "CAM DỪNG ĐỖ";
        case AlertKind::Animal: return "ĐỘNG VẬT";
        case AlertKind::ObjectOnRoad: return "VẬT CẢN";
        case AlertKind::Roadkill: return "ĐỘNG VẬT CHẾT";
        case AlertKind::Flood: return "NGẬP LỤT";
        case AlertKind::Fog: return "SƯƠNG MÙ";
        case AlertKind::Hail: return "MƯA ĐÁ";
        case AlertKind::Snow: return "TUYẾT";
        case AlertKind::Ice: return "BĂNG GIÁ";
        case AlertKind::SlipperyRoad: return "ĐƯỜNG TRƠN";
        case AlertKind::SpeedBump: return "GỜ GIẢM TỐC";
        case AlertKind::SchoolZone: return "TRƯỜNG HỌC";
        case AlertKind::LanesMerging: return "NHẬP LÀN";
        case AlertKind::DangerousCurve: return "ĐƯỜNG CONG";
        case AlertKind::Fork: return "NGÃ RẼ";
        case AlertKind::BrokenLight: return "ĐÈN HỎNG";
        case AlertKind::Cyclist: return "XE ĐẠP";
        case AlertKind::EmergencyVehicle: return "XE ƯU TIÊN";
        case AlertKind::PersonalSafety: return "AN TOÀN CÁ NHÂN";
        case AlertKind::NoStraightAndRight: return "CẤM THẲNG VÀ PHẢI";
        case AlertKind::NoLeftAndUTurn: return "CẤM TRÁI VÀ QUAY ĐẦU";
        case AlertKind::NoStraightAndLeft: return "CẤM THẲNG VÀ TRÁI";
        case AlertKind::NoLeftAndRight: return "CẤM TRÁI VÀ PHẢI";
        case AlertKind::CarNoLeftAndUTurn: return "Ô TÔ CẤM TRÁI VÀ QUAY ĐẦU";
        case AlertKind::CarNoRightAndUTurn: return "Ô TÔ CẤM PHẢI VÀ QUAY ĐẦU";
        case AlertKind::NoRightAndUTurn: return "CẤM PHẢI VÀ QUAY ĐẦU";
        case AlertKind::CarNoLeftTurn: return "Ô TÔ CẤM RẼ TRÁI";
        case AlertKind::CarNoRightTurn: return "Ô TÔ CẤM RẼ PHẢI";
        case AlertKind::CarNoUTurn: return "Ô TÔ CẤM QUAY ĐẦU";
        case AlertKind::TrafficLight: return "ĐÈN ĐỎ";
        default: return "CẢNH BÁO";
    }
}

void drawCard(Canvas &canvas, int x, int y, int w, int h, uint16_t borderCol) {
    canvas.line(x + 2, y, x + w - 3, y, borderCol);
    canvas.line(x + 2, y + h - 1, x + w - 3, y + h - 1, borderCol);
    canvas.line(x, y + 2, x, y + h - 3, borderCol);
    canvas.line(x + w - 1, y + 2, x + w - 1, y + h - 3, borderCol);
    canvas.pixel(x + 1, y + 1, borderCol);
    canvas.pixel(x + w - 2, y + 1, borderCol);
    canvas.pixel(x + 1, y + h - 2, borderCol);
    canvas.pixel(x + w - 2, y + h - 2, borderCol);
}

void drawAlertIcon(Canvas &canvas, int cx, int cy, int radius, const AlertState &alert, bool dominant) {
    if (alert.kind == AlertKind::None) return;
    const int iconSize = radius * 2;
    if (alert.kind == AlertKind::SpeedDrop) {
        const assets::ColorBitmap *sign = speedLimitAsset(
            alert.valueKmh, dominant ? SpeedSignContext::AlertLarge : SpeedSignContext::AlertSmall);
        if (sign && sign->pixels && sign->alpha) {
            if (dominant) {
                canvas.colorBitmapScaled(cx - iconSize / 2, cy - iconSize / 2, *sign, iconSize);
            } else {
                canvas.colorBitmap(cx - sign->width / 2, cy - sign->height / 2, *sign);
            }
            return;
        }
        const int thick = dominant ? 4 : 2;
        char value[5]{};
        canvas.fillCircle(cx,cy,radius,colors::White);
        canvas.circle(cx,cy,radius,colors::Red,thick);
        std::snprintf(value,sizeof(value),"%d",alert.valueKmh);
        if (dominant)
            canvas.fontText(cx-radius,cy-assets::kNumberMedium.lineHeight/2,value,
                            assets::kNumberMedium,colors::Black,2*radius,true);
        else
            canvas.fontText(cx-radius,cy-assets::kNumberSmall.lineHeight/2,value,
                            assets::kNumberSmall,colors::Black,2*radius,true);
        return;
    }

    const bool trafficJam = alert.kind == AlertKind::TrafficJam;
    const uint16_t severityColor = trafficSeverityColor(alert.trafficSeverity);
    if (trafficJam && alert.trafficSeverity > 0)
        canvas.circle(cx, cy, radius + (dominant ? 3 : 2), severityColor,
                      dominant ? 2 : 1);
    const assets::ColorBitmap *bitmap = trafficJam
        ? trafficJamAsset(alert.trafficSeverity, dominant) : alertAsset(alert.kind, dominant);
    if (!bitmap) bitmap = alertAsset(AlertKind::Hazard, dominant);
    if (bitmap && bitmap->pixels && bitmap->alpha) {
        if (dominant) {
            canvas.colorBitmapScaled(cx - iconSize / 2, cy - iconSize / 2, *bitmap, iconSize);
        } else {
            canvas.colorBitmap(cx - bitmap->width / 2, cy - bitmap->height / 2, *bitmap);
        }
        if (trafficJam && !dominant)
            drawTrafficSeverityTicks(canvas, cx, cy + radius + 3,
                                     alert.trafficSeverity, severityColor);
        return;
    }

    // Last-resort primitive only when the generated hazard asset is absent.
    canvas.triangle(cx,cy-radius,cx-radius,cy+radius,cx+radius,cy+radius,colors::Amber);
    canvas.fontText(cx-radius,cy-assets::kTextMedium.lineHeight/2,"!",assets::kTextMedium,
                    colors::Amber,2*radius,true);
}

void formatDistance(int meters, char *output, size_t capacity) {
    if (meters < 0) { output[0] = 0; return; }
    if (meters < 1000) std::snprintf(output, capacity, "%d M", meters);
    else if (meters < 10000) std::snprintf(output, capacity, "%.1f KM", meters / 1000.0);
    else std::snprintf(output, capacity, "%d KM", (meters + 500) / 1000);
}

inline int alertPriorityScore(AlertKind kind) {
    switch (kind) {
        // Hạng 1: Các loại biển cấm
        case AlertKind::NoPassing:
        case AlertKind::SpeedDrop:
        case AlertKind::NoCar:
        case AlertKind::NoMotorcycle:
        case AlertKind::NoLeftTurn:
        case AlertKind::NoRightTurn:
        case AlertKind::NoUTurn:
        case AlertKind::NoStraight:
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
            return 1;

        // Hạng 2: Camera, Đèn đỏ, Cảnh sát
        case AlertKind::SpeedCamera:
        case AlertKind::RedLightCamera:
        case AlertKind::TrafficLight:
        case AlertKind::Police:
        case AlertKind::PhoneCamera:
        case AlertKind::SeatbeltCamera:
        case AlertKind::DistanceCamera:
        case AlertKind::BusLaneCamera:
        case AlertKind::NoiseCamera:
        case AlertKind::StopSignCamera:
        case AlertKind::DummyCamera:
            return 2;

        // Hạng 3: Các cảnh báo chướng ngại, nguy hiểm, giao thông khác
        default:
            return (kind != AlertKind::None) ? 3 : 99;
    }
}

inline bool compareAlertPriority(const AlertState &a, const AlertState &b) {
    if (a.kind == AlertKind::None) return false;
    if (b.kind == AlertKind::None) return true;
    const int scoreA = alertPriorityScore(a.kind);
    const int scoreB = alertPriorityScore(b.kind);
    if (scoreA != scoreB) return scoreA < scoreB;
    return a.distanceM < b.distanceM;
}

uint8_t collectSortedAlerts(const HudState &state, AlertState *outAlerts, uint8_t maxCount) {
    uint8_t count = 0;
    if (state.noPassingZone && count < maxCount) {
        AlertState zone{};
        zone.kind = AlertKind::NoPassing;
        zone.distanceM = state.noPassingRemainingM;
        zone.valueKmh = 0;
        outAlerts[count++] = zone;
    }
    if (state.nearestAlert.kind != AlertKind::None) {
        bool dup = false;
        for (uint8_t i = 0; i < count; ++i) {
            if (outAlerts[i] == state.nearestAlert) { dup = true; break; }
        }
        if (!dup && count < maxCount) {
            outAlerts[count++] = state.nearestAlert;
        }
    }
    for (uint8_t i = 0; i < state.upcomingAlertCount && count < maxCount; ++i) {
        const auto &up = state.upcomingAlerts[i];
        if (up.kind == AlertKind::None) continue;
        bool dup = false;
        for (uint8_t j = 0; j < count; ++j) {
            if (outAlerts[j] == up) { dup = true; break; }
        }
        if (!dup && count < maxCount) {
            outAlerts[count++] = up;
        }
    }
    std::sort(outAlerts, outAlerts + count, compareAlertPriority);
    return count;
}
}  // namespace

esp_err_t HudRenderer::init() {
    buffer_ = static_cast<uint16_t *>(heap_caps_malloc(layout::MaxRegionPixels * sizeof(uint16_t),
                                                       MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (!buffer_) {
        ESP_LOGE(kTag, "Unable to allocate %d-byte dirty-region buffer",
                 layout::MaxRegionPixels * static_cast<int>(sizeof(uint16_t)));
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void HudRenderer::render(const HudState &state, const DeviceSettings &settings,
                         const SystemStatusSnapshot &systemStatus) {
    const int64_t currentClockMillis = localClockMillis(state);
    const int64_t currentClockSecond = currentClockMillis == INT64_MIN
        ? INT64_MIN : currentClockMillis / 1000LL;
    const int64_t currentClockMinute = currentClockSecond == INT64_MIN
        ? INT64_MIN : currentClockSecond / 60;
    const int8_t currentClockPhase = currentClockMillis == INT64_MIN
        ? -1 : static_cast<int8_t>((currentClockMillis % 1000LL) < 500LL);
    clockActive_ = state.connected && state.hasProducerState && currentClockSecond != INT64_MIN;
    const bool streetChanged = firstFrame_ || !sameText(state.currentStreet, previous_.currentStreet) ||
                               state.navigationActive != previous_.navigationActive;
    const int availableStreetWidth = 304;
    Canvas metrics(buffer_, layout::Street.width, layout::Street.height);
    char streetDisplayBuf[180]{};
    if (state.navigationActive) {
        std::snprintf(streetDisplayBuf, sizeof(streetDisplayBuf), "HT: %s", displayStreet(state));
    } else {
        std::snprintf(streetDisplayBuf, sizeof(streetDisplayBuf), "%s", displayStreet(state));
    }
    const int streetWidth = metrics.fontTextWidth(streetDisplayBuf, assets::kTextMedium);
    const bool shouldMarquee = state.connected && state.hasProducerState &&
                               streetWidth > availableStreetWidth;
    const uint64_t nowMs = static_cast<uint64_t>(esp_timer_get_time() / 1000);
    if (!shouldMarquee) {
        marqueeActive_ = false;
        marqueeOffset_ = 0;
    } else {
        if (!marqueeActive_ || streetChanged || streetWidth != marqueeTextWidth_ ||
            availableStreetWidth != marqueeAvailableWidth_) {
            marqueeEpochMs_ = nowMs;
            marqueeOffset_ = 0;
            marqueeRenderedOffset_ = -1;
        }
        marqueeActive_ = true;
        marqueeTextWidth_ = streetWidth;
        marqueeAvailableWidth_ = availableStreetWidth;
        constexpr uint64_t kStartHoldMs = 1200;
        constexpr uint64_t kEndHoldMs = 900;
        constexpr uint64_t kMsPerPixel = 45;
        const int overflow = streetWidth - availableStreetWidth;
        const uint64_t scrollMs = static_cast<uint64_t>(overflow) * kMsPerPixel;
        const uint64_t cycleMs = kStartHoldMs + scrollMs + kEndHoldMs;
        const uint64_t elapsed = cycleMs > 0 ? (nowMs - marqueeEpochMs_) % cycleMs : 0;
        if (elapsed < kStartHoldMs) marqueeOffset_ = 0;
        else if (elapsed < kStartHoldMs + scrollMs)
            marqueeOffset_ = std::min(overflow, static_cast<int>((elapsed - kStartHoldMs) / kMsPerPixel));
        else marqueeOffset_ = overflow;
    }
    const bool marqueeFrameChanged = marqueeActive_ && marqueeOffset_ != marqueeRenderedOffset_;
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    const bool nextStreetChanged = firstFrame_ || !sameText(state.nextStreet, previous_.nextStreet);
    constexpr int availableNextStreetWidth = 105;
    const int nextStreetWidth = state.nextStreet[0] != 0
        ? metrics.fontTextWidth(state.nextStreet.data(), assets::kTextMedium) : 0;
    const bool shouldNextStreetMarquee = state.connected && state.hasProducerState &&
                                         state.nextStreet[0] != 0 &&
                                         nextStreetWidth > availableNextStreetWidth;
    if (!shouldNextStreetMarquee) {
        nextStreetMarqueeActive_ = false;
        nextStreetMarqueeOffset_ = 0;
    } else {
        if (!nextStreetMarqueeActive_ || nextStreetChanged ||
            nextStreetWidth != nextStreetMarqueeTextWidth_) {
            nextStreetMarqueeEpochMs_ = nowMs;
            nextStreetMarqueeOffset_ = 0;
            nextStreetMarqueeRenderedOffset_ = -1;
        }
        nextStreetMarqueeActive_ = true;
        nextStreetMarqueeTextWidth_ = nextStreetWidth;
        constexpr uint64_t kNsStartHoldMs = 1500;
        constexpr uint64_t kNsEndHoldMs = 900;
        constexpr uint64_t kNsMsPerPixel = 45;
        const int overflow = nextStreetWidth - availableNextStreetWidth + 6;
        const uint64_t scrollMs = static_cast<uint64_t>(overflow) * kNsMsPerPixel;
        const uint64_t cycleMs = kNsStartHoldMs + scrollMs + kNsEndHoldMs;
        const uint64_t elapsed = cycleMs > 0 ? (nowMs - nextStreetMarqueeEpochMs_) % cycleMs : 0;
        if (elapsed < kNsStartHoldMs) nextStreetMarqueeOffset_ = 0;
        else if (elapsed < kNsStartHoldMs + scrollMs)
            nextStreetMarqueeOffset_ = std::min(overflow, static_cast<int>((elapsed - kNsStartHoldMs) / kNsMsPerPixel));
        else nextStreetMarqueeOffset_ = overflow;
    }
    const bool nextStreetMarqueeFrameChanged = nextStreetMarqueeActive_ &&
                                               nextStreetMarqueeOffset_ != nextStreetMarqueeRenderedOffset_;
#else
    constexpr bool nextStreetMarqueeFrameChanged = false;
#endif
    const bool isOverspeed = state.connected && state.hasProducerState &&
                             !state.signalStale &&
                             firmwareOverspeed(state, settings);
    overspeedActive_ = isOverspeed;
    const bool currentOverspeedPhase = isOverspeed && ((nowMs / 250ULL) % 2ULL == 0ULL);
    const bool overspeedPhaseChanged = currentOverspeedPhase != renderedOverspeedPhase_;
    if (overspeedPhaseChanged) {
        renderedOverspeedPhase_ = currentOverspeedPhase;
    }
    const bool statusChanged = firstFrame_ || state.connected != previous_.connected ||
                               state.hasProducerState != previous_.hasProducerState ||
                               state.navigationActive != previous_.navigationActive;
    const bool configChanged = firstFrame_ || hasSettingsChanged(settings, previousSettings_);
    uint8_t targetBrightness = 100;
    if (settings.fixedBrightness) {
        targetBrightness = settings.brightness;
    } else {
        if (currentClockMinute != INT64_MIN) {
            const int normalizedMinute = static_cast<int>((currentClockMinute % 1440 + 1440) % 1440);
            targetBrightness = calculateTimeOfDayBrightness(normalizedMinute, 100);
        } else if (settings.theme == UiTheme::Night) {
            targetBrightness = 20;
        }
    }

    if (firstFrame_ || targetBrightness != currentAppliedBrightness_) {
        const esp_err_t brightnessResult = DisplayDriver::instance().setBrightness(targetBrightness);
        if (brightnessResult != ESP_OK)
            ESP_LOGE(kTag, "Brightness update failed: %s", esp_err_to_name(brightnessResult));
        else
            currentAppliedBrightness_ = targetBrightness;
    }

    if (firstFrame_ || settings.invertColor != currentAppliedInvert_) {
        const esp_err_t invertResult = DisplayDriver::instance().setInvertColor(settings.invertColor);
        if (invertResult != ESP_OK)
            ESP_LOGE(kTag, "HUD color inversion update failed: %s", esp_err_to_name(invertResult));
        else
            currentAppliedInvert_ = settings.invertColor;
    }

    if (configChanged) {
        const esp_err_t orientationResult = DisplayDriver::instance().setOrientation(
            settings.mirrorHud, settings.rotateDisplay);
        if (orientationResult != ESP_OK)
            ESP_LOGE(kTag, "HUD orientation update failed: %s", esp_err_to_name(orientationResult));
    }
    const bool systemStatusChanged = firstFrame_ || systemStatus != previousSystemStatus_;
    if (systemStatus.visible) {
        if (systemStatusChanged || configChanged) {
            renderRegion(layout::Maneuver, state, settings, systemStatus);
            renderRegion(layout::SpeedCluster, state, settings, systemStatus);
            renderRegion(layout::Alerts, state, settings, systemStatus);
            renderRegion(layout::Guidance, state, settings, systemStatus);
            renderRegion(layout::Street, state, settings, systemStatus);
        }
        previous_ = state;
        previousSettings_ = settings;
        previousSystemStatus_ = systemStatus;
        firstFrame_ = false;
        return;
    }
    const bool systemStatusClosed = previousSystemStatus_.visible;
    bool streetRendered = false;
    bool maneuverRendered = false;
    const bool triggerFullRedraw = systemStatusClosed || statusChanged || configChanged ||
                                   !state.connected || !state.hasProducerState ||
                                   overspeedPhaseChanged;
    if (triggerFullRedraw) {
        renderRegion(layout::Maneuver, state, settings, systemStatus);
        maneuverRendered = true;
        renderRegion(layout::SpeedCluster, state, settings, systemStatus);
        renderRegion(layout::Alerts, state, settings, systemStatus);
        renderRegion(layout::Guidance, state, settings, systemStatus);
        renderRegion(layout::Street, state, settings, systemStatus);
        streetRendered = true;
    } else {
        const bool maneuverOrNextStreetChanged = maneuverChanged(state, previous_) || nextStreetMarqueeFrameChanged;
        if (maneuverOrNextStreetChanged) {
            renderRegion(layout::Maneuver, state, settings, systemStatus);
            maneuverRendered = true;
        }
        const bool speedClusterChanged = maneuverOrNextStreetChanged ||
                                        state.speedKmh != previous_.speedKmh ||
                                        state.speedLimitKmh != previous_.speedLimitKmh ||
                                        state.overSpeed != previous_.overSpeed ||
                                        currentClockMinute != renderedClockMinute_;
        if (speedClusterChanged)
            renderRegion(layout::SpeedCluster, state, settings, systemStatus);
        const bool laneStateSwapped = (state.laneCount > 0) != (previous_.laneCount > 0);
        const bool changedAlerts = alertsChanged(state, previous_) ||
                                   currentClockMinute != renderedClockMinute_ ||
                                   laneStateSwapped;
        if (changedAlerts) renderRegion(layout::Alerts, state, settings, systemStatus);
        if (guidanceChanged(state, previous_))
            renderRegion(layout::Guidance, state, settings, systemStatus);
        if (streetChanged || laneStateSwapped || marqueeFrameChanged) {
            renderRegion(layout::Street, state, settings, systemStatus);
            streetRendered = true;
        }
        if (systemStatusChanged) {
            renderRegion(layout::SpeedCluster, state, settings, systemStatus);
            renderRegion(layout::Alerts, state, settings, systemStatus);
        }
    }
    previous_ = state;
    previousSettings_ = settings;
    previousSystemStatus_ = systemStatus;
    renderedClockMinute_ = currentClockMinute;
    renderedClockPhase_ = currentClockPhase;
    if (streetRendered) marqueeRenderedOffset_ = marqueeOffset_;
    if (maneuverRendered) nextStreetMarqueeRenderedOffset_ = nextStreetMarqueeOffset_;
    firstFrame_ = false;
}

void renderOverspeedBorder(Canvas &canvas, const Rect &region) {
    constexpr int kBorder = 4;
    if (region.y == 0) {
        canvas.fillRect(0, 0, canvas.width(), kBorder, colors::Red);
    }
    if (region.y + region.height == layout::Height) {
        canvas.fillRect(0, canvas.height() - kBorder, canvas.width(), kBorder, colors::Red);
    }
    if (region.x == 0) {
        canvas.fillRect(0, 0, kBorder, canvas.height(), colors::Red);
    }
    if (region.x + region.width == layout::Width) {
        canvas.fillRect(canvas.width() - kBorder, 0, kBorder, canvas.height(), colors::Red);
    }
}

void HudRenderer::renderRegion(const Rect &region, const HudState &state,
                               const DeviceSettings &settings,
                               const SystemStatusSnapshot &systemStatus) {
    const Rect physicalRegion = layout::physicalRect(region);
    Canvas canvas(buffer_, physicalRegion.width, physicalRegion.height,
                  region.width, region.height);
    canvas.setTranslation(settings.offsetX, settings.offsetY);
    if (systemStatus.visible) renderSystemStatus(canvas, region, systemStatus, settings);
    else if (!state.connected || !state.hasProducerState) renderStatus(canvas, region, state, settings);
    else if (sameRegion(region, layout::Maneuver)) renderManeuver(canvas,state,settings);
    else if (sameRegion(region, layout::SpeedCluster)) renderSpeedCluster(canvas,state,settings);
    else if (sameRegion(region, layout::Alerts)) renderAlerts(canvas,state,settings);
    else if (sameRegion(region, layout::Guidance)) renderGuidance(canvas,state,settings);
    else renderStreet(canvas,state,settings);
    if (!systemStatus.visible && state.connected && state.hasProducerState) {
        renderMainIndicators(canvas, region, systemStatus);
        if (renderedOverspeedPhase_) {
            renderOverspeedBorder(canvas, region);
        }
    }
    const esp_err_t result = DisplayDriver::instance().drawRegion(physicalRegion, buffer_);
    if (result != ESP_OK) ESP_LOGE(kTag, "Dirty region (%d,%d %dx%d) failed: %s",
                                   region.x,region.y,region.width,region.height,esp_err_to_name(result));
}

void HudRenderer::renderMainIndicators(Canvas &canvas, const Rect &region,
                                       const SystemStatusSnapshot &systemStatus) {
    // Battery is centered across the upper HUD. It is omitted entirely when
    // GPIO4 does not contain a plausible single-cell LiPo voltage.
    if (systemStatus.batteryPresent && sameRegion(region, layout::SpeedCluster)) {
        constexpr int batteryX = 134;
        const int batteryY = mainY(5);
        constexpr int batteryWidth = 20;
        constexpr int batteryHeight = 11;
        const uint16_t batteryColor = systemStatus.batteryPercent <= 15 ? colors::Red
            : systemStatus.batteryPercent <= 35 ? colors::Amber : colors::Green;
        canvas.fillRect(batteryX - region.x, batteryY - region.y,
                        batteryWidth, batteryHeight, batteryColor);
        canvas.fillRect(batteryX + 2 - region.x, batteryY + 2 - region.y,
                        batteryWidth - 4, batteryHeight - 4, colors::Background);
        canvas.fillRect(batteryX + batteryWidth - region.x, batteryY + 3 - region.y,
                        3, batteryHeight - 6, batteryColor);
        const int fillWidth = (batteryWidth - 6) * systemStatus.batteryPercent / 100;
        canvas.fillRect(batteryX + 3 - region.x, batteryY + 3 - region.y,
                        fillWidth, batteryHeight - 6, batteryColor);
        char percent[8];
        std::snprintf(percent, sizeof(percent), "%u%%",
                      static_cast<unsigned>(systemStatus.batteryPercent));
        canvas.fontText(160 - region.x, mainY(0) - region.y, percent, assets::kTextSmall,
                        batteryColor, 38, false);
    }
}

void HudRenderer::renderSystemStatus(Canvas &canvas, const Rect &region,
                                     const SystemStatusSnapshot &systemStatus,
                                     const DeviceSettings &settings) {
    canvas.clear(colors::Background);
    const uint16_t fg = foreground(settings);
    canvas.fontText(0 - region.x, screenY(7) - region.y, "TRẠNG THÁI",
                    assets::kTextMedium, fg, layout::Width, true);

    // Battery body and terminal. The fill is proportional to the estimated
    // single-cell LiPo charge; an X marks an unavailable/non-battery reading.
    constexpr int batteryX = 25;
    const int batteryY = screenY(47);
    constexpr int batteryWidth = 52;
    constexpr int batteryHeight = 27;
    const uint16_t batteryColor = systemStatus.batteryPresent
        ? (systemStatus.batteryPercent <= 15 ? colors::Red
           : systemStatus.batteryPercent <= 35 ? colors::Amber : colors::Green)
        : colors::Muted;
    canvas.fillRect(batteryX - region.x, batteryY - region.y,
                    batteryWidth, batteryHeight, batteryColor);
    canvas.fillRect(batteryX + 3 - region.x, batteryY + 3 - region.y,
                    batteryWidth - 6, batteryHeight - 6, colors::Background);
    canvas.fillRect(batteryX + batteryWidth - region.x, batteryY + 8 - region.y,
                    5, batteryHeight - 16, batteryColor);
    if (systemStatus.batteryPresent) {
        const int fillWidth = (batteryWidth - 10) * systemStatus.batteryPercent / 100;
        canvas.fillRect(batteryX + 5 - region.x, batteryY + 5 - region.y,
                        fillWidth, batteryHeight - 10, batteryColor);
    } else {
        canvas.line(batteryX + 8 - region.x, batteryY + 6 - region.y,
                    batteryX + batteryWidth - 8 - region.x,
                    batteryY + batteryHeight - 6 - region.y, colors::Red, 3);
        canvas.line(batteryX + batteryWidth - 8 - region.x, batteryY + 6 - region.y,
                    batteryX + 8 - region.x,
                    batteryY + batteryHeight - 6 - region.y, colors::Red, 3);
    }
    char batteryText[24];
    if (systemStatus.batteryPresent)
        std::snprintf(batteryText, sizeof(batteryText), "PIN %u%%",
                      static_cast<unsigned>(systemStatus.batteryPercent));
    else
        std::snprintf(batteryText, sizeof(batteryText), "KHÔNG CÓ PIN");
    canvas.fontText(95 - region.x, screenY(49) - region.y, batteryText,
                    assets::kTextMedium,
                    batteryColor, 215, false);

#if __has_include("serial/serial_transport.h")
    // USB connection indicator
    const uint16_t usbColor = transportColor(systemStatus);
    canvas.fontText(25 - region.x, screenY(99) - region.y, "USB",
                    assets::kTextMedium, usbColor, 60, false);
    const char *usbText = systemStatus.transportConnected
        ? "USB ĐÃ KẾT NỐI" : "USB CHƯA CÓ DỮ LIỆU";
    canvas.fontText(95 - region.x, screenY(99) - region.y, usbText,
                    assets::kTextMedium, usbColor, 210, false);
#else
    // Bluetooth rune plus four qualitative signal bars.
    constexpr int bluetoothX = 49;
    const int bluetoothTop = screenY(92);
    const int bluetoothBottom = screenY(132);
    const uint16_t bluetoothColor = bleSignalColor(systemStatus);
    canvas.line(bluetoothX - region.x, bluetoothTop - region.y,
                bluetoothX - region.x, bluetoothBottom - region.y, bluetoothColor, 3);
    canvas.line(bluetoothX - region.x, bluetoothTop - region.y,
                bluetoothX + 13 - region.x, screenY(103) - region.y, bluetoothColor, 3);
    canvas.line(bluetoothX + 13 - region.x, screenY(103) - region.y,
                bluetoothX - 10 - region.x, screenY(122) - region.y, bluetoothColor, 3);
    canvas.line(bluetoothX - 10 - region.x, screenY(101) - region.y,
                bluetoothX + 13 - region.x, screenY(122) - region.y, bluetoothColor, 3);
    canvas.line(bluetoothX + 13 - region.x, screenY(122) - region.y,
                bluetoothX - region.x, bluetoothBottom - region.y, bluetoothColor, 3);

    int signalBars = 0;
    if (systemStatus.bleConnected) {
        signalBars = systemStatus.bleRssiDbm >= -55 ? 4 :
                     systemStatus.bleRssiDbm >= -67 ? 3 :
                     systemStatus.bleRssiDbm >= -78 ? 2 :
                     systemStatus.bleRssiDbm >= -90 ? 1 : 0;
    }
    for (int bar = 0; bar < 4; ++bar) {
        const int height = 5 + bar * 5;
        canvas.fillRect(69 + bar * 6 - region.x, screenY(132) - height - region.y,
                        4, height, bar < signalBars ? bluetoothColor : colors::Muted);
    }
    char bleText[28];
    if (systemStatus.bleConnected)
        std::snprintf(bleText, sizeof(bleText), "BLE %d dBm",
                      static_cast<int>(systemStatus.bleRssiDbm));
    else
        std::snprintf(bleText, sizeof(bleText), "BLE CHƯA KẾT NỐI");
    canvas.fontText(100 - region.x, screenY(102) - region.y, bleText,
                    assets::kTextMedium, bluetoothColor, 210, false);
#endif
}

#ifndef WAZE_HUD_FIRMWARE_VERSION
#define WAZE_HUD_FIRMWARE_VERSION "2.8.2"
#endif

void HudRenderer::renderStatus(Canvas &canvas, const Rect &region, const HudState &state, const DeviceSettings &settings) {
    canvas.clear(colors::Background);

    // Header bar (sy = 0..34)
    canvas.fillRect(0 - region.x, 0 - region.y, layout::Width, 34, colors::Panel);
    canvas.fillRect(0 - region.x, 34 - region.y, layout::Width, 1, colors::Muted);
    canvas.fontText(12 - region.x, 8 - region.y, "WAZE HUD", assets::kTextLarge, colors::White, 140, false);

#if __has_include("serial/serial_transport.h")
    constexpr bool kIsUsb = true;
#else
    constexpr bool kIsUsb = false;
#endif

    // Status badge on the right
    const char *statusText = state.signalStale ? "MẤT TÍN HIỆU" : state.connected ? "ĐÃ KẾT NỐI" : (kIsUsb ? "CHỜ CÁP USB" : "CHỜ BLUETOOTH");
    const uint16_t statusColor = state.signalStale ? colors::Amber : state.connected ? colors::Green : (kIsUsb ? colors::Amber : colors::Cyan);
    constexpr int badgeW = 120;
    const int badgeX = layout::Width - badgeW - 8;
    canvas.fillRect(badgeX - region.x, 6 - region.y, badgeW, 22, colors::Panel);
    canvas.line(badgeX - region.x, 6 - region.y, badgeX + badgeW - region.x, 6 - region.y, statusColor);
    canvas.line(badgeX - region.x, 28 - region.y, badgeX + badgeW - region.x, 28 - region.y, statusColor);
    canvas.line(badgeX - region.x, 6 - region.y, badgeX - region.x, 28 - region.y, statusColor);
    canvas.line(badgeX + badgeW - region.x, 6 - region.y, badgeX + badgeW - region.x, 28 - region.y, statusColor);
    canvas.fillCircle(badgeX + 9 - region.x, 17 - region.y, 3, statusColor);
    canvas.fontText(badgeX + 16 - region.x, 9 - region.y, statusText, assets::kTextSmall, colors::White, badgeW - 20, false);

    // Instructions Card Box (sy = 42..188)
    constexpr int cardX = 10;
    constexpr int cardY = 42;
    constexpr int cardW = 300;
    constexpr int cardH = 146;
    canvas.fillRect(cardX - region.x, cardY - region.y, cardW, cardH, colors::Panel);
    canvas.line(cardX - region.x, cardY - region.y, cardX + cardW - region.x, cardY - region.y, colors::Muted);
    canvas.line(cardX - region.x, cardY + cardH - region.y, cardX + cardW - region.x, cardY + cardH - region.y, colors::Muted);
    canvas.line(cardX - region.x, cardY - region.y, cardX - region.x, cardY + cardH - region.y, colors::Muted);
    canvas.line(cardX + cardW - region.x, cardY - region.y, cardX + cardW - region.x, cardY + cardH - region.y, colors::Muted);

    if (state.signalStale) {
        canvas.fontText(cardX + 15 - region.x, cardY + 25 - region.y, "Mất kết nối với điện thoại", assets::kTextMedium, colors::Amber, cardW - 30, false);
        canvas.fontText(cardX + 15 - region.x, cardY + 60 - region.y, "Đang tự động kết nối lại...", assets::kTextSmall, colors::White, cardW - 30, false);
        canvas.fontText(cardX + 15 - region.x, cardY + 85 - region.y, "Kiểm tra lại Bluetooth / Cáp trên máy", assets::kTextSmall, colors::Muted, cardW - 30, false);
    } else if (state.connected) {
        canvas.fontText(cardX + 15 - region.x, cardY + 25 - region.y, "Đã kết nối thành công!", assets::kTextMedium, colors::Green, cardW - 30, false);
        canvas.fontText(cardX + 15 - region.x, cardY + 60 - region.y, "Đang chờ WazeMod gửi dữ liệu lộ trình...", assets::kTextSmall, colors::White, cardW - 30, false);
        canvas.fontText(cardX + 15 - region.x, cardY + 85 - region.y, "Bắt đầu chuyến đi trên điện thoại để hiện HUD", assets::kTextSmall, colors::Muted, cardW - 30, false);
    } else {
        canvas.fontText(cardX + 10 - region.x, cardY + 8 - region.y, "CÁCH KẾT NỐI VỚI ĐIỆN THOẠI:", assets::kTextSmall, colors::Amber, cardW - 20, false);
        const uint16_t numColor = kIsUsb ? colors::Amber : colors::Cyan;

        // Step 1
        canvas.fillRect(cardX + 10 - region.x, cardY + 32 - region.y, 20, 20, numColor);
        canvas.fontText(cardX + 16 - region.x, cardY + 34 - region.y, "1", assets::kTextSmall, colors::Background, 10, false);
        const char *step1 = kIsUsb ? "Cắm cáp OTG vào cổng USB của HUD" : "Bật Bluetooth trên điện thoại";
        canvas.fontText(cardX + 38 - region.x, cardY + 34 - region.y, step1, assets::kTextSmall, colors::White, cardW - 46, false);

        // Step 2
        canvas.fillRect(cardX + 10 - region.x, cardY + 68 - region.y, 20, 20, numColor);
        canvas.fontText(cardX + 16 - region.x, cardY + 70 - region.y, "2", assets::kTextSmall, colors::Background, 10, false);
        canvas.fontText(cardX + 38 - region.x, cardY + 70 - region.y, "Mở ứng dụng WazeMod", assets::kTextSmall, colors::White, cardW - 46, false);

        // Step 3
        canvas.fillRect(cardX + 10 - region.x, cardY + 104 - region.y, 20, 20, numColor);
        canvas.fontText(cardX + 16 - region.x, cardY + 106 - region.y, "3", assets::kTextSmall, colors::Background, 10, false);
        const char *step3 = kIsUsb ? "Cài đặt MOD -> Kết nối với thiết bị (USB)" : "Cài đặt MOD -> Kết nối với thiết bị";
        canvas.fontText(cardX + 38 - region.x, cardY + 106 - region.y, step3, assets::kTextSmall, colors::White, cardW - 46, false);
    }

    // Footer hints (sy = 196..236)
    const char *subhint = kIsUsb ? "Baudrate: 115200 bps | Cấp quyền USB" : "Tên thiết bị BLE: WazeHUD";
    canvas.fontText(0 - region.x, 198 - region.y, subhint, assets::kTextSmall, colors::Cyan, layout::Width, true);

    canvas.fontText(0 - region.x, 218 - region.y, "wazemod.io.vn - f38 VOZ", assets::kTextSmall, colors::Muted, layout::Width, true);
}

void HudRenderer::renderManeuver(Canvas &canvas, const HudState &state, const DeviceSettings &settings) {
    canvas.clear(colors::Panel);
    const uint16_t fg = colors::White;
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    if (!state.navigationActive || state.maneuver == Maneuver::None) {
        canvas.fontText(4, 16, "CHẠY", assets::kTextSmall, colors::Muted, 77, true);
        canvas.fontText(4, 34, "TỰ DO", assets::kTextMedium, colors::Green, 77, true);
        canvas.circle(42, 80, 18, colors::Muted, 1);
        canvas.fillCircle(42, 80, 3, colors::Green);
        canvas.line(42, 66, 42, 75, colors::Red, 2);
        canvas.fontText(4, 106, "CRUISE", assets::kTextSmall, colors::Muted, 77, true);
        return;
    }
    if (state.nextStreet[0] != 0) {
        const int offset = nextStreetMarqueeActive_ ? nextStreetMarqueeOffset_ : 0;
        canvas.fontText(5 - offset, 4, state.nextStreet.data(),
                        assets::kTextMedium, colors::White, -1, false);
    }
    drawManeuverIcon(canvas, state.maneuver, state.roundaboutExit, fg);
    char distance[16]; formatDistance(state.maneuverDistanceM, distance, sizeof(distance));
    canvas.fontText(2, 98, distance, assets::kTextMedium, colors::White, 81, true);
#else
    if (!state.navigationActive || state.maneuver == Maneuver::None) return;
    drawManeuverIcon(canvas,state.maneuver,state.roundaboutExit,fg);
    char distance[16]; formatDistance(state.maneuverDistanceM,distance,sizeof(distance));
    canvas.fontText(2, mainY(108), distance, assets::kTextSmall, fg, 81, true);
#endif
}

void HudRenderer::renderSpeedCluster(Canvas &canvas, const HudState &state,
                                     const DeviceSettings &settings) {
    canvas.clear(colors::Panel);

#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    constexpr int signX = 70;
    constexpr int signY = 62;
    constexpr int outerRadius = 60;
    constexpr int innerRadius = 50;
    constexpr int badgeW = 38;
    constexpr int badgeH = 28;
    constexpr int badgeX = 140 - badgeW - 2; // 100
    constexpr int badgeY = 130 - badgeH - 2; // 100
#else
    constexpr int signX = 64;
    const int signY = mainY(58);
    constexpr int outerRadius = 54;
    constexpr int innerRadius = 45;
    constexpr int badgeX = 92;
    const int badgeY = mainY(94);
    constexpr int badgeW = 44;
    constexpr int badgeH = 32;
#endif

    // Kiểm tra chế độ ban đêm: Tuân theo Giao diện (Theme) từ cấu hình hoặc giờ tự động
    bool isNight = false;
    if (settings.theme == UiTheme::Night) {
        isNight = true;
    } else if (settings.theme == UiTheme::Day) {
        isNight = false;
    } else {
        const int64_t clockMs = localClockMillis(state);
        if (clockMs != INT64_MIN) {
            const int64_t minute = (clockMs / 1000LL) / 60;
            const int normMin = static_cast<int>((minute % 1440 + 1440) % 1440);
            // Khung giờ giảm độ sáng tự động: 17:30 -> 06:00
            isNight = (normMin >= 17 * 60 + 30 || normMin < 6 * 60);
        }
    }

    const uint16_t signBgColor = isNight ? colors::Panel : colors::White;
    const uint16_t signTextColor = isNight ? colors::White : colors::Black;

#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    if (state.navigationActive && state.maneuver != Maneuver::None && state.nextStreet[0] != 0) {
        const int offset = nextStreetMarqueeActive_ ? nextStreetMarqueeOffset_ : 0;
        canvas.fontText(5 - layout::Maneuver.width - offset, 4, state.nextStreet.data(),
                        assets::kTextMedium, colors::White, -1, false);
        canvas.fillRect(signX - 33, 0, canvas.width() - (signX - 33), 30, colors::Panel);
        canvas.fillCircle(signX, signY, outerRadius + 2, colors::Panel);
    }
#endif

    canvas.fillCircle(signX, signY, outerRadius, colors::Red);
    canvas.fillCircle(signX, signY, innerRadius, signBgColor);

    if (state.speedLimitKmh > 0) {
        char limit[16];
        std::snprintf(limit, sizeof(limit), "%d", state.speedLimitKmh);
        // Tốc độ >= 100 (3 chữ số): font kNumberSpeedLimit3 (size 36) chuẩn nét, không chạm viền
        // Tốc độ < 100 (2 chữ số): font kNumberSpeedLimit (size 48) số TO đậm, sắc nét 100% native TrueType
        const auto &speedFont = (state.speedLimitKmh >= 100) ? assets::kNumberSpeedLimit3 : assets::kNumberSpeedLimit;
        canvas.fontText(signX - innerRadius, signY - speedFont.lineHeight / 2 - 1,
                        limit, speedFont, signTextColor, innerRadius * 2, true);
    } else {
        canvas.fontText(signX - innerRadius, signY - assets::kNumberSpeedLimit.lineHeight / 2 - 1,
                        "?", assets::kNumberSpeedLimit, signTextColor, innerRadius * 2, true);
    }

    // Car speed badge in bottom-right corner
    canvas.fillRect(badgeX, badgeY, badgeW, badgeH, colors::Panel);
    drawCard(canvas, badgeX, badgeY, badgeW, badgeH, colors::Muted);

    char speed[16];
    std::snprintf(speed, sizeof(speed), "%d", std::clamp(state.speedKmh, 0, 999));
    const uint16_t speedColor = firmwareOverspeed(state, settings)
        ? (renderedOverspeedPhase_ ? colors::Red : colors::White)
        : foreground(settings);

    const auto &speedFont = (state.speedKmh >= 100) ? assets::kNumberSmall : assets::kNumberMedium;
    canvas.fontText(badgeX, badgeY + (badgeH - speedFont.lineHeight) / 2,
                    speed, speedFont, speedColor, badgeW, true);
}

void HudRenderer::renderAlerts(Canvas &canvas, const HudState &state, const DeviceSettings &settings) {
    canvas.clear(colors::Background);

    const int64_t millis = localClockMillis(state);
    const int64_t second = millis == INT64_MIN ? INT64_MIN : millis / 1000LL;
    if (second != INT64_MIN) {
        const int64_t minute = second / 60;
        const int normalizedMinute = static_cast<int>((minute % 1440 + 1440) % 1440);
        char clock[8];
        std::snprintf(clock, sizeof(clock), "%02d:%02d",
                      normalizedMinute / 60, normalizedMinute % 60);
        const int clockWidth = canvas.fontTextWidth(clock, assets::kTextMedium);
        const int clockX = canvas.width() - clockWidth - 5;
        canvas.fontText(clockX, 4, clock, assets::kTextMedium, colors::White, -1, false);
    }

    AlertState allAlerts[8];
    const uint8_t alertCount = collectSortedAlerts(state, allAlerts, 8);
    const bool hasLanes = (state.laneCount > 0);

    if (alertCount == 0) {
        return;
    }

    if (!hasLanes) {
        // TRẠNG THÁI 1: Không có lane (Chạy tự do hoặc đi đường thẳng không có lane)
        // Cụm bên phải hiển thị DUY NHẤT 1 cảnh báo ưu tiên cao nhất (VIP),
        // các cảnh báo tiếp theo đã được đẩy xuống thanh đáy mở rộng.
        const auto &primary = allAlerts[0];
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
        constexpr int iconRadius = 20;
        constexpr int iconY = 48;
        constexpr int textY = 74;
#else
        constexpr int iconRadius = 22;
        const int iconY = mainY(42);
        const int textY = mainY(68);
#endif
        drawAlertIcon(canvas, 47, iconY, iconRadius, primary, true);
        char distance[16]; formatDistance(primary.distanceM, distance, sizeof(distance));
        canvas.fontText(2, textY, distance, assets::kTextMedium,
                        alertDistanceColor(primary.distanceM, colors::White), 91, true);

        if (primary.kind == AlertKind::TrafficJam) {
            char trafficDetail[48];
            if (primary.trafficDelayMinutes >= 0)
                std::snprintf(trafficDetail, sizeof(trafficDetail), "%.20s +%d PH",
                              trafficSeverityLabel(primary.trafficSeverity),
                              primary.trafficDelayMinutes);
            else
                std::snprintf(trafficDetail, sizeof(trafficDetail), "%.20s",
                              trafficSeverityLabel(primary.trafficSeverity));
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
            canvas.fontText(1, 96, trafficDetail, assets::kTextSmall,
                            trafficSeverityColor(primary.trafficSeverity), 93, true);
#else
            canvas.fontText(1, mainY(86), trafficDetail, assets::kTextSmall,
                            trafficSeverityColor(primary.trafficSeverity), 93, true);
#endif
        }
    } else {
        // TRẠNG THÁI 2: Đang có lane (Thanh đáy bận hiển thị mũi tên phân làn)
        // Cụm bên phải hiển thị 2 tầng (trên to, dưới nhỏ) theo thứ tự ưu tiên
        const auto &primary = allAlerts[0];
        const bool hasSecondary = (alertCount > 1);
        const auto &upcoming = hasSecondary ? allAlerts[1] : AlertState{};

#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
        const int iconRadius = hasSecondary ? 16 : 20;
        const int iconY = hasSecondary ? 42 : 48;
        const int textY = hasSecondary ? 64 : 74;
#else
        const int iconRadius = hasSecondary ? 18 : 22;
        const int iconY = mainY(hasSecondary ? 34 : 42);
        const int textY = mainY(hasSecondary ? 58 : 68);
#endif
        drawAlertIcon(canvas, 47, iconY, iconRadius, primary, true);
        char distance[16]; formatDistance(primary.distanceM, distance, sizeof(distance));
        const auto &distFont = hasSecondary ? assets::kTextSmall : assets::kTextMedium;
        canvas.fontText(2, textY, distance, distFont,
                        alertDistanceColor(primary.distanceM, colors::White), 91, true);

        if (primary.kind == AlertKind::TrafficJam) {
            char trafficDetail[48];
            if (primary.trafficDelayMinutes >= 0)
                std::snprintf(trafficDetail, sizeof(trafficDetail), "%.20s +%d PH",
                              trafficSeverityLabel(primary.trafficSeverity),
                              primary.trafficDelayMinutes);
            else
                std::snprintf(trafficDetail, sizeof(trafficDetail), "%.20s",
                              trafficSeverityLabel(primary.trafficSeverity));
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
            const int labelY = hasSecondary ? 78 : 96;
            canvas.fontText(1, labelY, trafficDetail, assets::kTextSmall,
                            trafficSeverityColor(primary.trafficSeverity), 93, true);
#else
            const int labelY = hasSecondary ? mainY(74) : mainY(86);
            canvas.fontText(1, labelY, trafficDetail, assets::kTextSmall,
                            trafficSeverityColor(primary.trafficSeverity), 93, true);
#endif
        }

        if (hasSecondary) {
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
            constexpr int secondaryIconY = 94;
            constexpr int secondaryTextY = 110;
#else
            const int secondaryIconY = mainY(105);
            const int secondaryTextY = mainY(121);
#endif
            drawAlertIcon(canvas, 47, secondaryIconY, 13, upcoming, false);
            char upcomingDistance[12]; formatDistance(upcoming.distanceM, upcomingDistance, sizeof(upcomingDistance));
            canvas.fontText(2, secondaryTextY, upcomingDistance, assets::kTextSmall,
                            alertDistanceColor(upcoming.distanceM, colors::White), 91, true);
        }
    }
}

void HudRenderer::renderGuidance(Canvas &canvas, const HudState &state,
                                 const DeviceSettings &settings) {
    canvas.clear(colors::Panel);
    const uint16_t fg = colors::White;

    const uint8_t totalLanes = std::min<uint8_t>(state.laneCount, kMaxLanes);
    if (totalLanes > 0) {
        constexpr uint8_t kMaxVisibleLanes = 8;
        uint8_t startIdx = 0;
        uint8_t visibleCount = totalLanes;

        if (totalLanes > kMaxVisibleLanes) {
            int firstSel = -1;
            int lastSel = -1;
            for (uint8_t i = 0; i < totalLanes; ++i) {
                if (state.lanes[i].selectedMask != 0) {
                    if (firstSel < 0) firstSel = i;
                    lastSel = i;
                }
            }
            if (firstSel >= 0) {
                const int centerSel = (firstSel + lastSel) / 2;
                int s = centerSel - kMaxVisibleLanes / 2;
                startIdx = static_cast<uint8_t>(std::clamp(s, 0, static_cast<int>(totalLanes - kMaxVisibleLanes)));
            } else {
                startIdx = 0;
            }
            visibleCount = kMaxVisibleLanes;
        }

        // Draw indicator if there are hidden lanes on the left
        if (startIdx > 0) {
            drawArrowHeadDir(canvas, 6, 41, 2, colors::Amber, 6);
        }
        // Draw indicator if there are hidden lanes on the right
        if (startIdx + visibleCount < totalLanes) {
            drawArrowHeadDir(canvas, layout::Width - 6, 41, 3, colors::Amber, 6);
        }

        const int margin = (startIdx > 0 || startIdx + visibleCount < totalLanes) ? 20 : 6;
        const int available = layout::Width - 2 * margin;
        const int spacing = std::min(55, available / static_cast<int>(visibleCount));
        const int totalWidth = spacing * static_cast<int>(visibleCount);
        const int firstX = (layout::Width - totalWidth) / 2 + spacing / 2;

        for (uint8_t i = 0; i < visibleCount; ++i) {
            const uint8_t laneIdx = startIdx + i;
            const int lx = firstX + i * spacing;
            drawGuidanceLane(canvas, lx, spacing, state.lanes[laneIdx], fg);
            if (i > 0) {
                const int divX = firstX + i * spacing - spacing / 2;
                for (int dy = 16; dy < 68; dy += 10) {
                    canvas.line(divX, dy, divX, dy + 5, colors::Muted, 1);
                }
            }
        }
    } else {
        AlertState allAlerts[8];
        const uint8_t totalAlerts = collectSortedAlerts(state, allAlerts, 8);

        // allAlerts[0] (cảnh báo quan trọng nhất) đã được hiển thị ở Cụm bên phải (vị trí VIP nhìn trước).
        // Thanh đáy chỉ hiển thị các cảnh báo tiếp theo, tuyệt đối không bị trùng lặp!
        const AlertState *subsequentAlerts = allAlerts + 1;
        const uint8_t alertCount = (totalAlerts > 1) ? std::min<uint8_t>(4, totalAlerts - 1) : 0;

        if (alertCount > 0) {
            if (alertCount == 1) {
                const auto &alert = subsequentAlerts[0];
                drawCard(canvas, 10, 8, 300, 66, colors::Muted);
                drawAlertIcon(canvas, 45, 41, 20, alert, true);
                canvas.fontText(80, 16, alertKindLabel(alert.kind), assets::kTextMedium, colors::White, 210, false);
                char distance[16]; formatDistance(alert.distanceM, distance, sizeof(distance));
                canvas.fontText(80, 42, distance, assets::kTextLarge, alertDistanceColor(alert.distanceM, colors::Cyan), 120, false);
                if (alert.kind == AlertKind::TrafficJam && alert.trafficDelayMinutes >= 0) {
                    char trafficDetail[48];
                    std::snprintf(trafficDetail, sizeof(trafficDetail), "+%d PH (%s)",
                                  alert.trafficDelayMinutes, trafficSeverityLabel(alert.trafficSeverity));
                    canvas.fontText(190, 44, trafficDetail, assets::kTextSmall, trafficSeverityColor(alert.trafficSeverity), 110, false);
                }
            } else if (alertCount == 2) {
                constexpr int cardW = 146;
                constexpr int cardH = 68;
                for (int i = 0; i < 2; ++i) {
                    const int cx1 = 10 + i * 154;
                    const auto &alert = subsequentAlerts[i];
                    drawCard(canvas, cx1, 7, cardW, cardH, colors::Muted);
                    drawAlertIcon(canvas, cx1 + 26, 41, 16, alert, i == 0);
                    canvas.fontText(cx1 + 50, 16, alertKindLabel(alert.kind), assets::kTextSmall, colors::White, cardW - 54, false);
                    char distance[16]; formatDistance(alert.distanceM, distance, sizeof(distance));
                    canvas.fontText(cx1 + 50, 40, distance, assets::kTextMedium, alertDistanceColor(alert.distanceM, colors::Cyan), cardW - 54, false);
                }
            } else if (alertCount == 3) {
                constexpr int cardW = 98;
                constexpr int cardH = 68;
                for (int i = 0; i < 3; ++i) {
                    const int cx1 = 7 + i * 103;
                    const auto &alert = subsequentAlerts[i];
                    drawCard(canvas, cx1, 7, cardW, cardH, colors::Muted);
                    drawAlertIcon(canvas, cx1 + cardW / 2, 24, 14, alert, i == 0);
                    char distance[16]; formatDistance(alert.distanceM, distance, sizeof(distance));
                    canvas.fontText(cx1 + 2, 42, distance, assets::kTextSmall, alertDistanceColor(alert.distanceM, colors::Cyan), cardW - 4, true);
                    canvas.fontText(cx1 + 2, 56, alertKindLabel(alert.kind), assets::kTextSmall, colors::White, cardW - 4, true);
                }
            } else if (alertCount >= 4) {
                constexpr int cardW = 73;
                constexpr int cardH = 68;
                for (int i = 0; i < 4; ++i) {
                    const int cx1 = 6 + i * 78;
                    const auto &alert = subsequentAlerts[i];
                    drawCard(canvas, cx1, 7, cardW, cardH, colors::Muted);
                    drawAlertIcon(canvas, cx1 + cardW / 2, 24, 13, alert, i == 0);
                    char distance[16]; formatDistance(alert.distanceM, distance, sizeof(distance));
                    canvas.fontText(cx1 + 2, 42, distance, assets::kTextSmall, alertDistanceColor(alert.distanceM, colors::Cyan), cardW - 4, true);
                    canvas.fontText(cx1 + 2, 56, alertKindLabel(alert.kind), assets::kTextSmall, colors::White, cardW - 4, true);
                }
            }
        } else if (state.navigationActive && (state.eta[0] != 0 || state.remainingMinutes > 0 || state.remainingKm > 0.0F)) {
            constexpr int etaWidth = 85;
            canvas.fillRect(etaWidth - 1, 6, 1, layout::GuidanceHeight - 12, colors::Muted);

            if (state.eta[0] != 0) {
                canvas.fontText(0, 8, "ETA", assets::kTextSmall, colors::White, etaWidth - 2, true);
                canvas.fontText(0, 36, state.eta.data(), assets::kTextLarge, colors::White, etaWidth - 2, true);
            }

            if (state.remainingMinutes > 0 || state.remainingKm > 0) {
                char distBuf[16]{};
                if (state.remainingKm >= 1.0F) {
                    std::snprintf(distBuf, sizeof(distBuf), "%.1f km", state.remainingKm);
                } else if (state.remainingMeters > 0) {
                    std::snprintf(distBuf, sizeof(distBuf), "%d m", state.remainingMeters);
                }
                char timeBuf[16]{};
                if (state.remainingMinutes >= 60) {
                    std::snprintf(timeBuf, sizeof(timeBuf), "%dh %02dp",
                                  state.remainingMinutes / 60, state.remainingMinutes % 60);
                } else if (state.remainingMinutes > 0) {
                    std::snprintf(timeBuf, sizeof(timeBuf), "%d ph", state.remainingMinutes);
                }
                if (distBuf[0] != 0) {
                    canvas.fontText(98, 8, "CÒN LẠI", assets::kTextSmall, colors::White, 100, false);
                    canvas.fontText(98, 36, distBuf, assets::kTextMedium, colors::White, 100, false);
                }
                if (timeBuf[0] != 0) {
                    canvas.fontText(210, 8, "THỜI GIAN", assets::kTextSmall, colors::White, 100, false);
                    canvas.fontText(210, 36, timeBuf, assets::kTextMedium, colors::White, 100, false);
                }
            }
        } else {
            drawCard(canvas, 16, 12, 288, 58, colors::Muted);
            canvas.fillCircle(45, 41, 14, colors::Green);
            canvas.fontText(72, 22, "LỘ TRÌNH AN TOÀN", assets::kTextMedium, colors::Green, 220, false);
            canvas.fontText(72, 44, "Không có cảnh báo phía trước", assets::kTextSmall, colors::Muted, 220, false);
        }
    }
}

void HudRenderer::renderStreet(Canvas &canvas, const HudState &state, const DeviceSettings &settings) {
    (void)settings;
    canvas.clear(colors::Panel);
    const int textY = std::max(0, (layout::StreetHeight - assets::kTextMedium.lineHeight) / 2);
    char streetBuf[180]{};
    if (state.navigationActive) {
        std::snprintf(streetBuf, sizeof(streetBuf), "HT: %s", displayStreet(state));
    } else {
        std::snprintf(streetBuf, sizeof(streetBuf), "%s", displayStreet(state));
    }

    if (state.laneCount > 0 && state.eta[0] != 0) {
        char etaBuf[16];
        std::snprintf(etaBuf, sizeof(etaBuf), "ETA %s", state.eta.data());
        const int etaW = canvas.fontTextWidth(etaBuf, assets::kTextSmall) + 6;
        canvas.fontText(8, textY, streetBuf, assets::kTextSmall, colors::White, layout::Width - etaW - 16, false);
        canvas.fontText(layout::Width - etaW - 4, textY, etaBuf, assets::kTextSmall, colors::Cyan, etaW, false);
    } else {
        if (marqueeActive_) {
            canvas.fontText(8 - marqueeOffset_, textY, streetBuf, assets::kTextMedium,
                            colors::White, -1, false);
        } else {
            if (state.navigationActive) {
                canvas.fontText(8, textY, streetBuf, assets::kTextMedium, colors::White,
                                304, false);
            } else {
                canvas.fontText(5, textY, streetBuf, assets::kTextMedium, colors::White,
                                310, true);
            }
        }
    }
}

}  // namespace waze_hud
