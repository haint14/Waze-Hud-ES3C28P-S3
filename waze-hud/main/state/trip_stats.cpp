#include "state/trip_stats.h"

#include "esp_timer.h"
#include <algorithm>
#include <cmath>

namespace waze_hud {
namespace {
// A longer gap between samples means the stream stalled; do not count it.
constexpr int64_t kMaxSampleGapUs = 1500000;
// Do not show an average until there is enough data for it to be meaningful.
constexpr int64_t kMinAverageUs = 5000000;
}  // namespace

void TripStats::update(const HudState &state) {
    const bool live = state.connected && state.hasProducerState && !state.signalStale;
    if (!live) {
        lastUs_ = 0;
        return;
    }
    const int64_t now = esp_timer_get_time();
    const int speedKmh = std::clamp(state.speedKmh, 0, 999);
    if (lastUs_ != 0) {
        const int64_t gap = now - lastUs_;
        if (gap > 0 && gap <= kMaxSampleGapUs) {
            elapsedUs_ += gap;
            meters_ += (speedKmh / 3.6) * (static_cast<double>(gap) / 1e6);
        }
    }
    lastUs_ = now;
    maxKmh_ = std::max(maxKmh_, speedKmh);
}

TripDisplay TripStats::display() const {
    TripDisplay result;
    result.distanceTenthsKm = static_cast<int>(meters_ / 100.0);
    result.minutes = static_cast<int>(elapsedUs_ / 60000000LL);
    result.maxKmh = maxKmh_;
    if (elapsedUs_ >= kMinAverageUs) {
        const double seconds = static_cast<double>(elapsedUs_) / 1e6;
        result.avgKmh = static_cast<int>(std::lround(meters_ / seconds * 3.6));
    }
    return result;
}

}  // namespace waze_hud
