#include "application/FeedingService.h"

#include "domain/EventId.h"

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

    // Pending-queue drain on offline → online edge. Events that were
    // logged while offline accumulated in /pending.jsonl via
    // storage_.enqueue; replay each through the network. Failed
    // replays re-enqueue so the next online edge tries again. Silent
    // by design — application/ stays Arduino-agnostic; if you need
    // a breadcrumb, watch for [net] POST /api/feed lines.
    const bool onlineNow = network_.isOnline();
    if (onlineNow && !wasOnline_) {
        auto pending = storage_.drainPending();
        for (auto& ev : pending) {
            const bool sent = (ev.type == "snooze")
                ? network_.postSnooze(ev.by, ev.ts, /*durationSec=*/0,
                                       ev.cat, ev.clientEventId)
                : network_.postFeed(ev.by, ev.ts, ev.cat, ev.clientEventId);
            if (!sent) storage_.enqueue(ev);
        }
    }
    wasOnline_ = onlineNow;

    // Per-cat network poll. Every NETWORK_POLL_INTERVAL_SEC, walk the
    // roster and fetch each cat's state from the backend. Each cat's
    // state merges only if remote is at least as new (handles the
    // "this device just fed → don't clobber with stale remote" case).
    // Cost: N tiny HTTPS GETs per 30 s. Trivial at N≤4.
    if (network_.isOnline() &&
        now - lastNetworkPollSec_ >= NETWORK_POLL_INTERVAL_SEC) {
        lastNetworkPollSec_ = now;
        for (int slot = 0; slot < roster_.count(); ++slot) {
            const uint8_t catId = roster_.at(slot).id;
            if (auto fresh = network_.fetchState(catId)) {
                if (fresh->lastFeedTs >= states_[slot].lastFeedTs) {
                    states_[slot].lastFeedTs    = fresh->lastFeedTs;
                    states_[slot].todayCount    = fresh->todayCount;
                    states_[slot].snoozeUntilTs = fresh->snoozeUntilTs;
                }
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
    ev.clientEventId = feedme::domain::generateEventId();
    ev.ts   = now;
    ev.type = "feed";
    ev.by   = by ? by : "";
    ev.cat  = roster_.at(catSlot).id;
    storage_.enqueue(ev);
    storage_.recordHistory(ev);
    network_.postFeed(ev.by, now, ev.cat, ev.clientEventId);
    appendHistory(now, "feed", by, ev.cat);
}

void FeedingService::snooze(const char* by, int durationSec, int catSlot) {
    if (catSlot < 0 || catSlot >= roster_.count()) return;
    const int64_t now = clock_.nowSec();
    states_[catSlot].snoozeUntilTs = now + durationSec;

    feedme::ports::PendingEvent ev{};
    ev.clientEventId = feedme::domain::generateEventId();
    ev.ts   = now;
    ev.type = "snooze";
    ev.by   = by ? by : "";
    ev.cat  = roster_.at(catSlot).id;
    storage_.enqueue(ev);
    storage_.recordHistory(ev);
    network_.postSnooze(ev.by, now, durationSec, ev.cat, ev.clientEventId);
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
