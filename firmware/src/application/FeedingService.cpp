#include "application/FeedingService.h"

namespace feedme::application {

namespace {
constexpr int FED_BANNER_SEC            = 3;
constexpr int NETWORK_POLL_INTERVAL_SEC = 30;
// Returned for out-of-range slot queries so callers can treat the
// reference as a constant zero-initialised state.
const feedme::domain::FeedingState kEmptyState{};
}

FeedingService::FeedingService(feedme::ports::IClock& clock,
                               feedme::ports::INetwork& network,
                               feedme::ports::IStorage& storage,
                               feedme::domain::CatRoster& roster)
    : clock_(clock), network_(network), storage_(storage), roster_(roster) {}

void FeedingService::tick() {
    const int64_t now = clock_.nowSec();

    // Per-cat justFed banner expiry — independent timers per cat, so
    // feeding two cats back-to-back doesn't cut one banner short.
    for (int i = 0; i < roster_.count(); ++i) {
        if (states_[i].justFed && now >= justFedClearAt_[i]) {
            states_[i].justFed = false;
        }
    }

    // Network poll — for now NoopNetwork ignores the cat parameter and
    // returns nothing. When WifiNetwork lands (Phase 2.1) it'll need a
    // per-cat fetch endpoint; this is a placeholder hook.
    if (network_.isOnline() &&
        now - lastNetworkPollSec_ >= NETWORK_POLL_INTERVAL_SEC) {
        lastNetworkPollSec_ = now;
        if (auto fresh = network_.fetchState()) {
            // Pre-multi-cat NoopNetwork returns a single FeedingState
            // — assume slot 0. Once the network learns about cats it
            // should emit per-cat snapshots and we'll route here.
            if (roster_.count() > 0
                && fresh->lastFeedTs >= states_[0].lastFeedTs) {
                states_[0].lastFeedTs    = fresh->lastFeedTs;
                states_[0].todayCount    = fresh->todayCount;
                states_[0].snoozeUntilTs = fresh->snoozeUntilTs;
            }
        }
    }
}

void FeedingService::logFeeding(const char* by, int catSlot) {
    if (catSlot < 0 || catSlot >= roster_.count()) return;
    const int64_t now = clock_.nowSec();
    auto& s = states_[catSlot];
    s.lastFeedTs    = now;
    s.todayCount   += 1;
    s.snoozeUntilTs = 0;
    s.justFed       = true;
    justFedClearAt_[catSlot] = now + FED_BANNER_SEC;

    feedme::ports::PendingEvent ev{};
    ev.ts   = now;
    ev.type = "feed";
    ev.by   = by ? by : "";
    ev.cat  = roster_.at(catSlot).id;
    storage_.enqueue(ev);
    storage_.recordHistory(ev);
    network_.postFeed(ev.by, now);
    appendHistory(now, "feed", by, ev.cat);
}

void FeedingService::snooze(const char* by, int durationSec, int catSlot) {
    if (catSlot < 0 || catSlot >= roster_.count()) return;
    const int64_t now = clock_.nowSec();
    states_[catSlot].snoozeUntilTs = now + durationSec;

    feedme::ports::PendingEvent ev{};
    ev.ts   = now;
    ev.type = "snooze";
    ev.by   = by ? by : "";
    ev.cat  = roster_.at(catSlot).id;
    storage_.enqueue(ev);
    storage_.recordHistory(ev);
    network_.postSnooze(ev.by, now, durationSec);
    appendHistory(now, "snooze", by, ev.cat);
}

void FeedingService::loadHistoryFromStorage() {
    auto recent = storage_.loadRecentHistory(HISTORY_CAPACITY);
    // recent is newest-first; replay oldest-first into the ring buffer
    // so the ordering inside history_ matches what live appends produce.
    for (auto it = recent.rbegin(); it != recent.rend(); ++it) {
        appendHistory(it->ts, it->type.c_str(), it->by.c_str(), it->cat);
    }
    // Reseed each cat's lastFeedTs from the most recent feed event
    // attributed to that cat. Walk newest-first; first hit wins per cat.
    std::array<bool, feedme::domain::CatRoster::MAX_CATS> seeded{};
    for (const auto& ev : recent) {
        if (ev.type != "feed") continue;
        int slot = roster_.findSlotById(ev.cat);
        // Events from a deleted cat (or a pre-multi-cat era with cat=0
        // when slot 0 doesn't have id 0) fall through to slot 0 as a
        // last-resort default — better than dropping the seed.
        if (slot < 0) slot = 0;
        if (slot >= roster_.count()) continue;
        if (seeded[slot]) continue;
        states_[slot].lastFeedTs = ev.ts;
        seeded[slot] = true;
    }
}

void FeedingService::appendHistory(int64_t ts, const char* type,
                                   const char* by, uint8_t cat) {
    auto& slot = history_[historyHead_];
    slot.ts   = ts;
    slot.type = type ? type : "";
    slot.by   = by   ? by   : "";
    slot.cat  = cat;
    historyHead_ = (historyHead_ + 1) % HISTORY_CAPACITY;
    if (historyCount_ < HISTORY_CAPACITY) ++historyCount_;
}

const feedme::domain::FeedingState& FeedingService::state(int catSlot) const {
    if (catSlot < 0 || catSlot >= roster_.count()) return kEmptyState;
    return states_[catSlot];
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
