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
    storage_.recordHistory(ev);
    network_.postFeed(ev.by, now);
    appendHistory(now, "feed", by);
}

void FeedingService::snooze(const char* by, int durationSec) {
    const int64_t now = clock_.nowSec();
    state_.snoozeUntilTs = now + durationSec;

    feedme::ports::PendingEvent ev{};
    ev.ts = now;
    ev.type = "snooze";
    ev.by = by ? by : "";
    storage_.enqueue(ev);
    storage_.recordHistory(ev);
    network_.postSnooze(ev.by, now, durationSec);
    appendHistory(now, "snooze", by);
}

void FeedingService::loadHistoryFromStorage() {
    auto recent = storage_.loadRecentHistory(HISTORY_CAPACITY);
    // recent is newest-first; replay oldest-first into the ring buffer
    // so the ordering inside history_ matches what live appends produce.
    for (auto it = recent.rbegin(); it != recent.rend(); ++it) {
        appendHistory(it->ts, it->type.c_str(), it->by.c_str());
    }
    // Best-effort: reseed lastFeedTs/todayCount from the most recent
    // feed event so the ring/mood reflect prior-session activity.
    for (const auto& ev : recent) {
        if (ev.type == "feed") {
            state_.lastFeedTs = ev.ts;
            break;
        }
    }
}

void FeedingService::appendHistory(int64_t ts, const char* type, const char* by) {
    auto& slot = history_[historyHead_];
    slot.ts   = ts;
    slot.type = type ? type : "";
    slot.by   = by   ? by   : "";
    historyHead_ = (historyHead_ + 1) % HISTORY_CAPACITY;
    if (historyCount_ < HISTORY_CAPACITY) ++historyCount_;
}

size_t FeedingService::copyRecentEvents(
    std::array<HistoryEntry, HISTORY_CAPACITY>& out) const {
    // Newest-first: start from the slot just before head and walk back.
    size_t idx = historyHead_;
    for (size_t i = 0; i < historyCount_; ++i) {
        idx = (idx == 0 ? HISTORY_CAPACITY : idx) - 1;
        out[i] = history_[idx];
    }
    return historyCount_;
}

}  // namespace feedme::application
