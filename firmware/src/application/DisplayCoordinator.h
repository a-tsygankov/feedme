#pragma once

#include "application/FeedingService.h"
#include "ports/IClock.h"
#include "ports/IDisplay.h"

namespace feedme::application {

// Glue between FeedingService (state) and IDisplay (pixels).
// Does NOT call hardware directly — it builds a DisplayFrame and asks the
// adapter to render it.
class DisplayCoordinator {
public:
    DisplayCoordinator(feedme::ports::IDisplay& display,
                       FeedingService& feeding,
                       feedme::ports::IClock& clock,
                       int64_t hungryThresholdSec);

    // Call from loop(). Cheap; the display adapter diffs internally.
    void tick();

private:
    feedme::ports::IDisplay& display_;
    FeedingService&          feeding_;
    feedme::ports::IClock&   clock_;
    int64_t                  hungryThresholdSec_;
};

}  // namespace feedme::application
