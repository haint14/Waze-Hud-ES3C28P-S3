#pragma once

#include "state/hud_state.h"
#include <cstdint>

namespace waze_hud {

// Values shown on the free-drive trip card, already rounded for display so
// that equality means "nothing to redraw".
struct TripDisplay {
    int distanceTenthsKm{0};
    int minutes{0};
    int maxKmh{0};
    int avgKmh{0};

    bool operator==(const TripDisplay &other) const {
        return distanceTenthsKm == other.distanceTenthsKm && minutes == other.minutes &&
               maxKmh == other.maxKmh && avgKmh == other.avgKmh;
    }
};

// Trip statistics integrated from the vehicle speed in the HLP stream. They
// accumulate only while fresh producer data is arriving (a gap in the stream
// pauses the trip instead of being counted) and reset when the HUD restarts.
class TripStats {
public:
    void update(const HudState &state);
    TripDisplay display() const;

private:
    double meters_{0.0};
    int64_t elapsedUs_{0};
    int maxKmh_{0};
    int64_t lastUs_{0};  // 0 = no live sample yet
};

}  // namespace waze_hud
