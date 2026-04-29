#pragma once

#include "application/FeedingService.h"
#include "domain/CatRoster.h"
#include "domain/TimeZone.h"
#include "ports/IClock.h"
#include "ports/IDisplay.h"

namespace feedme::application {

// Glue between FeedingService (state) and IDisplay (pixels).
// Does NOT call hardware directly — it builds a DisplayFrame and asks the
// adapter to render it.
//
// Hungry threshold is per-cat (Phase E.x — moved out of this class
// into Cat). Adjustments route through CatRoster::setActiveThresholdSec
// which marks the roster dirty; main.cpp's roster persist handler
// writes it to NVS on the next service tick.
class DisplayCoordinator {
public:
    DisplayCoordinator(feedme::ports::IDisplay& display,
                       FeedingService& feeding,
                       feedme::ports::IClock& clock,
                       feedme::domain::CatRoster& roster,
                       const feedme::domain::TimeZone& tz);

    // Call from loop(). Cheap; the display adapter diffs internally.
    void tick();

    // Adjust the active cat's hungry-mood threshold by a delta in
    // seconds. Clamped to the [MIN, MAX] range. Returns the new value.
    int64_t adjustHungryThreshold(int64_t deltaSec);
    int64_t hungryThresholdSec() const { return roster_.activeThresholdSec(); }

    static constexpr int64_t MIN_THRESHOLD_SEC = 30  * 60;  // 30 minutes
    static constexpr int64_t MAX_THRESHOLD_SEC = 12 * 3600; // 12 hours

private:
    feedme::ports::IDisplay&        display_;
    FeedingService&                 feeding_;
    feedme::ports::IClock&          clock_;
    feedme::domain::CatRoster&      roster_;
    const feedme::domain::TimeZone& tz_;
};

}  // namespace feedme::application
