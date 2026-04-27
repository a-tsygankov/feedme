#pragma once

#include "application/FeedingService.h"
#include "ports/IClock.h"
#include "ports/IDisplay.h"
#include "ports/IPreferences.h"

namespace feedme::application {

// Glue between FeedingService (state) and IDisplay (pixels).
// Does NOT call hardware directly — it builds a DisplayFrame and asks the
// adapter to render it.
class DisplayCoordinator {
public:
    DisplayCoordinator(feedme::ports::IDisplay& display,
                       FeedingService& feeding,
                       feedme::ports::IClock& clock,
                       feedme::ports::IPreferences& prefs,
                       int64_t hungryThresholdSec);

    // Call from loop(). Cheap; the display adapter diffs internally.
    void tick();

    // Pull the persisted threshold out of prefs into our in-memory
    // value. Call once in setup() after prefs.begin() so the static-
    // init ordering doesn't bite (constructor runs during global init,
    // before prefs.begin() can open NVS).
    void loadPreferences();

    // Adjust the hungry-mood threshold by a delta in seconds. Clamped
    // to the [MIN_THRESHOLD_SEC, MAX_THRESHOLD_SEC] range. Persists to
    // prefs immediately. Returns the new value so the caller can
    // log/display it.
    int64_t adjustHungryThreshold(int64_t deltaSec);
    int64_t hungryThresholdSec() const { return hungryThresholdSec_; }

    static constexpr int64_t MIN_THRESHOLD_SEC = 30  * 60;  // 30 minutes
    static constexpr int64_t MAX_THRESHOLD_SEC = 12 * 3600; // 12 hours

private:
    feedme::ports::IDisplay&     display_;
    FeedingService&              feeding_;
    feedme::ports::IClock&       clock_;
    feedme::ports::IPreferences& prefs_;
    int64_t                      hungryThresholdSec_;
};

}  // namespace feedme::application
