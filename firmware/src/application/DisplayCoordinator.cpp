#include "application/DisplayCoordinator.h"

#include "domain/MoodCalculator.h"
#include "domain/RingProgress.h"

namespace feedme::application {

DisplayCoordinator::DisplayCoordinator(feedme::ports::IDisplay& display,
                                       FeedingService& feeding,
                                       feedme::ports::IClock& clock,
                                       feedme::ports::IPreferences& prefs,
                                       int64_t hungryThresholdSec)
    : display_(display),
      feeding_(feeding),
      clock_(clock),
      prefs_(prefs),
      hungryThresholdSec_(hungryThresholdSec) {}

void DisplayCoordinator::loadPreferences() {
    hungryThresholdSec_ = prefs_.getHungryThresholdSec(hungryThresholdSec_);
}

int64_t DisplayCoordinator::adjustHungryThreshold(int64_t deltaSec) {
    int64_t v = hungryThresholdSec_ + deltaSec;
    if (v < MIN_THRESHOLD_SEC) v = MIN_THRESHOLD_SEC;
    if (v > MAX_THRESHOLD_SEC) v = MAX_THRESHOLD_SEC;
    hungryThresholdSec_ = v;
    prefs_.setHungryThresholdSec(v);
    return v;
}

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
