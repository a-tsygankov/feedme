#include "application/DisplayCoordinator.h"

#include "domain/MoodCalculator.h"
#include "domain/RingProgress.h"

#include <stdio.h>
#include <string.h>

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
    const int activeSlot = roster_.activeCatIdx();
    const auto& s = feeding_.state(activeSlot);
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

    // Attribution of the most recent feed for the *active cat*'s "fed
    // by …" label. Multi-cat: showing some other cat's feeder here
    // would mismatch the "fed Xm ago" age (which is per-active-cat).
    // Walk newest-first; first feed event matching the active cat's
    // stable id wins.
    frame.lastFedBy[0] = '\0';
    if (activeSlot >= 0 && activeSlot < roster_.count()) {
        const uint8_t activeId = roster_.at(activeSlot).id;
        std::array<FeedingService::HistoryEntry,
                   FeedingService::HISTORY_CAPACITY> recent;
        const size_t n = feeding_.copyRecentEvents(recent);
        for (size_t i = 0; i < n; ++i) {
            if (recent[i].cat != activeId)        continue;
            if (recent[i].type != "feed")         continue;
            if (recent[i].by.empty())             continue;
            const size_t cap = sizeof(frame.lastFedBy) - 1;
            const size_t copyLen = recent[i].by.size() < cap
                                   ? recent[i].by.size() : cap;
            memcpy(frame.lastFedBy, recent[i].by.data(), copyLen);
            frame.lastFedBy[copyLen] = '\0';
            break;
        }
    }

    display_.render(frame);
    display_.tick();
}

}  // namespace feedme::application
