#include "application/DisplayCoordinator.h"

#include "domain/MoodCalculator.h"
#include "domain/RingProgress.h"

namespace feedme::application {

DisplayCoordinator::DisplayCoordinator(feedme::ports::IDisplay& display,
                                       FeedingService& feeding,
                                       feedme::ports::IClock& clock,
                                       int64_t hungryThresholdSec)
    : display_(display),
      feeding_(feeding),
      clock_(clock),
      hungryThresholdSec_(hungryThresholdSec) {}

void DisplayCoordinator::tick() {
    const auto& s = feeding_.state();
    const int64_t now = clock_.nowSec();

    feedme::ports::DisplayFrame frame{};
    frame.mood = feedme::domain::calculateMood(s, now, hungryThresholdSec_);
    frame.ringProgress = feedme::domain::computeRingProgress(s, now, hungryThresholdSec_);
    frame.todayCount = s.todayCount;
    frame.minutesSinceFeed = (s.lastFeedTs == 0)
        ? -1
        : static_cast<int>((now - s.lastFeedTs) / 60);
    display_.render(frame);
    display_.tick();
}

}  // namespace feedme::application
