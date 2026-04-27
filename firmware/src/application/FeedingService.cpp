#include "application/FeedingService.h"

namespace feedme::application {

namespace {
constexpr int FED_BANNER_SEC = 3;
constexpr int NETWORK_POLL_INTERVAL_SEC = 30;
}

FeedingService::FeedingService(feedme::ports::IClock& clock,
                               feedme::ports::INetwork& network,
                               feedme::ports::IStorage& storage)
    : clock_(clock), network_(network), storage_(storage) {}

void FeedingService::tick() {
    const int64_t now = clock_.nowSec();

    if (state_.justFed && now >= justFedClearAt_) {
        state_.justFed = false;
    }

    if (network_.isOnline() &&
        now - lastNetworkPollSec_ >= NETWORK_POLL_INTERVAL_SEC) {
        lastNetworkPollSec_ = now;
        if (auto fresh = network_.fetchState()) {
            // Only adopt remote state if it's newer than ours.
            if (fresh->lastFeedTs >= state_.lastFeedTs) {
                state_.lastFeedTs    = fresh->lastFeedTs;
                state_.todayCount    = fresh->todayCount;
                state_.snoozeUntilTs = fresh->snoozeUntilTs;
            }
        }
    }
}

void FeedingService::logFeeding(const char* by) {
    const int64_t now = clock_.nowSec();
    state_.lastFeedTs = now;
    state_.todayCount += 1;
    state_.snoozeUntilTs = 0;
    state_.justFed = true;
    justFedClearAt_ = now + FED_BANNER_SEC;

    feedme::ports::PendingEvent ev{};
    ev.ts = now;
    ev.type = "feed";
    ev.by = by ? by : "";
    storage_.enqueue(ev);
    network_.postFeed(ev.by, now);
}

void FeedingService::snooze(const char* by, int durationSec) {
    const int64_t now = clock_.nowSec();
    state_.snoozeUntilTs = now + durationSec;

    feedme::ports::PendingEvent ev{};
    ev.ts = now;
    ev.type = "snooze";
    ev.by = by ? by : "";
    storage_.enqueue(ev);
    network_.postSnooze(ev.by, now, durationSec);
}

}  // namespace feedme::application
