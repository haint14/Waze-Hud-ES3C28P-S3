#pragma once

#include "sdkconfig.h"
#include <cstdint>

namespace waze_hud {

struct Rect {
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
};

namespace layout {
#if CONFIG_WAZE_HUD_DISPLAY_35_480X320
constexpr bool IsLargeDisplay = true;
constexpr const char *DeviceName = "ESP32-S3 3.5-inch HUD";
constexpr int UiYScaleNumerator = 5;
constexpr int UiYScaleDenominator = 4;
constexpr int Width = 320;
constexpr int Height = 213;
constexpr int PhysicalWidth = 480;
constexpr int PhysicalHeight = 320;
// 173 logical rows scale to exactly 260 physical rows. This keeps the
// main/street boundary four-pixel aligned after landscape pixels are rotated
// into the ST77922 native 320x480 address space.
constexpr int MainHeight = 173;
constexpr int GuidanceHeight = 0;
constexpr int StreetHeight = Height - MainHeight;
#elif CONFIG_WAZE_HUD_DISPLAY_CYD_28
constexpr bool IsLargeDisplay = false;
constexpr const char *DeviceName = "ESP32-2432S028 2.8-inch HUD";
// Use a native 320x240 canvas. Only layout positions are spread vertically;
// bitmap assets and fonts remain square, unscaled pixels.
constexpr int UiYScaleNumerator = 7;
constexpr int UiYScaleDenominator = 5;
constexpr int Width = 320;
constexpr int Height = 240;
constexpr int PhysicalWidth = 320;
constexpr int PhysicalHeight = 240;
constexpr int MainHeight = 130;
constexpr int StreetHeight = 28;
constexpr int GuidanceHeight = Height - MainHeight - StreetHeight;
#else
constexpr bool IsLargeDisplay = false;
constexpr const char *DeviceName = "LILYGO T-Display-S3";
constexpr int UiYScaleNumerator = 1;
constexpr int UiYScaleDenominator = 1;
constexpr int Width = 320;
constexpr int Height = 170;
constexpr int PhysicalWidth = 320;
constexpr int PhysicalHeight = 170;
constexpr int MainHeight = 140;
constexpr int StreetHeight = Height - MainHeight;
constexpr int GuidanceHeight = 0;
#endif

constexpr Rect Maneuver{0, 0, 85, MainHeight};
#if CONFIG_WAZE_HUD_DISPLAY_35_480X320
// These logical widths map to landscape x={0,128,336,480}.
constexpr Rect SpeedCluster{85, 0, 139, MainHeight};
constexpr Rect Alerts{224, 0, 96, MainHeight};
#else
constexpr Rect SpeedCluster{85, 0, 140, MainHeight};
constexpr Rect MainCluster{0, 0, 225, MainHeight};
constexpr Rect Alerts{225, 0, 95, MainHeight};
#endif
constexpr Rect Street{0, MainHeight, Width, StreetHeight};
constexpr Rect Guidance{0, MainHeight + StreetHeight, Width, GuidanceHeight};
constexpr Rect Full{0, 0, Width, Height};

constexpr int scaleCoordinate(int value, int logicalExtent, int physicalExtent) {
    return (value * physicalExtent + logicalExtent / 2) / logicalExtent;
}

constexpr Rect physicalRect(const Rect &logical) {
    const int x0 = scaleCoordinate(logical.x, Width, PhysicalWidth);
    const int y0 = scaleCoordinate(logical.y, Height, PhysicalHeight);
    const int x1 = scaleCoordinate(logical.x + logical.width, Width, PhysicalWidth);
    const int y1 = scaleCoordinate(logical.y + logical.height, Height, PhysicalHeight);
    return {static_cast<int16_t>(x0), static_cast<int16_t>(y0),
            static_cast<int16_t>(x1 - x0), static_cast<int16_t>(y1 - y0)};
}

constexpr int regionPixels(const Rect &logical) {
    const Rect physical = physicalRect(logical);
    return physical.width * physical.height;
}

constexpr int maxInt(int left, int right) { return left > right ? left : right; }
constexpr int MaxRegionPixels = maxInt(
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
    regionPixels(MainCluster),
#else
    maxInt(regionPixels(Maneuver), regionPixels(SpeedCluster)),
#endif
    maxInt(regionPixels(Alerts),
           maxInt(regionPixels(Guidance), regionPixels(Street))));

static_assert(physicalRect(Full).x == 0 && physicalRect(Full).y == 0,
              "Display viewport must start at the framebuffer origin");
static_assert(physicalRect(Full).width == PhysicalWidth &&
              physicalRect(Full).height == PhysicalHeight,
              "Display viewport must cover the complete framebuffer");
#if CONFIG_WAZE_HUD_DISPLAY_CYD_28
static_assert(physicalRect(MainCluster).width + physicalRect(Alerts).width == PhysicalWidth,
              "HUD columns must cover the framebuffer without gaps");
#else
static_assert(physicalRect(Maneuver).width + physicalRect(SpeedCluster).width +
              physicalRect(Alerts).width == PhysicalWidth,
              "HUD columns must cover the framebuffer without gaps");
#endif
static_assert(physicalRect(Maneuver).height + physicalRect(Guidance).height +
              physicalRect(Street).height == PhysicalHeight,
              "HUD rows must cover the framebuffer without gaps");
#if CONFIG_WAZE_HUD_DISPLAY_35_480X320
static_assert(physicalRect(Maneuver).x % 4 == 0 &&
              (physicalRect(Maneuver).x + physicalRect(Maneuver).width) % 4 == 0 &&
              physicalRect(SpeedCluster).x % 4 == 0 &&
              (physicalRect(SpeedCluster).x + physicalRect(SpeedCluster).width) % 4 == 0 &&
              physicalRect(Alerts).x % 4 == 0 &&
              (physicalRect(Alerts).x + physicalRect(Alerts).width) % 4 == 0,
              "Landscape dirty-region X coordinates must be four-pixel aligned");
static_assert(physicalRect(Maneuver).y % 4 == 0 &&
              (physicalRect(Maneuver).y + physicalRect(Maneuver).height) % 4 == 0 &&
              physicalRect(Street).y % 4 == 0 &&
              (physicalRect(Street).y + physicalRect(Street).height) % 4 == 0,
              "Rotated ST77922 native X boundaries must be four-pixel aligned");
#endif
}  // namespace layout

}  // namespace waze_hud
