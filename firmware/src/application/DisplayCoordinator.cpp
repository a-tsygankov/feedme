#include "application/DisplayCoordinator.h"

#include "domain/MoodCalculator.h"
#include "domain/RingProgress.h"

namespace feedme::application {

DisplayCoordinator::DisplayCoordinator(feedme::ports::IDisplay& display,
                                       FeedingService& feeding,
                                       feedme::ports::IClock& clock,
                                       feedme::domain::CatRoster& roster)
    : display_(display),
      feeding_(feeding),
      clock_(clock),
      roster_(roster) {}

int64_t DisplayCoordinator::adjustHungryThreshold(int64_t deltaSec) {
    int64_t v = roster_.activeThresholdSec() + deltaSec;
    if (v < MIN_THRESHOLD_SEC) v = MIN_THRESHOLD_SEC;
    if (v > MAX_THRESHOLD_SEC) v = MAX_THRESHOLD_SEC;
    roster_.setActiveThresholdSec(v);
    return v;
}

void DisplayCoordinator::tick() {
    const auto& s = feeding_.state();
    const int64_t now = clock_.nowSec();
    const int64_t threshold = roster_.activeThresholdSec();

    feedme::ports::DisplayFrame frame{};
    frame.mood = feedme::domain::calculateMood(s, now, threshold);
    frame.ringProgress = feedme::domain::computeRingProgress(s, now, threshold);
    frame.todayCount = s.todayCount;
    frame.minutesSinceFeed = (s.lastFeedTs == 0)
        ? -1
        : static_cast<int>((now - s.lastFeedTs) / 60);

    // Wall-clock time for the Idle screen header. Modulo 86400 so the
    // device shows a sensible time even before NTP syncs (clock falls
    // back to millis/1000 in ArduinoClock — wraps every 24 h).
    const int64_t secondsToday = ((now % 86400) + 86400) % 86400;
    frame.hour   = static_cast<int>(secondsToday / 3600);
    frame.minute = static_cast<int>((secondsToday % 3600) / 60);

    display_.render(frame);
    display_.tick();
}

}  // namespace feedme::application
