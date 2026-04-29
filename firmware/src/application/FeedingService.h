#pragma once

#include "domain/CatRoster.h"
#include "domain/FeedingState.h"
#include "ports/IClock.h"
#include "ports/INetwork.h"
#include "ports/IStorage.h"

#include <array>
#include <string>

namespace feedme::application {

// Owns the canonical per-cat FeedingState array. Mediates between user
// input, the clock, the network, and offline storage. Knows nothing
// about the display — DisplayCoordinator subscribes to it.
//
// Multi-cat (Phase E.x): each cat in the household keeps its own
// FeedingState (lastFeedTs, todayCount, snoozeUntilTs, justFed) so the
// Idle screen reflects the active cat. Events stamp Cat::id (stable
// across renames) at the storage/network boundary; in-memory state is
// indexed by roster slot for cheap access. Cats added after this
// device's last boot may show up in event history without a matching
// roster entry — those events route to slot 0 as a safe default.
class FeedingService {
public:
    static constexpr size_t HISTORY_CAPACITY = 5;

    struct HistoryEntry {
        int64_t     ts = 0;        // unix seconds
        std::string type;          // "feed" | "snooze"
        std::string by;
        uint8_t     cat = 0;       // Cat::id at the moment of the event
    };

    FeedingService(feedme::ports::IClock& clock,
                   feedme::ports::INetwork& network,
                   feedme::ports::IStorage& storage,
                   feedme::domain::CatRoster& roster);

    // Idempotent. Pulls latest from network if available; otherwise
    // advances each cat's "justFed" flag etc. Call from loop() at ~1 Hz.
    void tick();

    // Tap actions. Each takes the cat slot index (typically the active
    // cat) so multi-cat households attribute correctly.
    void logFeeding(const char* by, int catSlot);
    void snooze    (const char* by, int durationSec, int catSlot);

    // Pull persisted history out of storage into the in-memory ring
    // buffer + per-cat states. Call once after storage.begin() so the
    // history view shows entries from previous boots and each cat's
    // state reflects its own last feed.
    void loadHistoryFromStorage();

    // Per-cat state accessor. Out-of-range slots return a static empty
    // state to keep callers branch-light — DisplayCoordinator does
    // bounds checking via roster.count() before it reaches us.
    const feedme::domain::FeedingState& state(int catSlot) const;

    // Most-recent-first iteration over the in-memory history ring
    // buffer (global across cats — UX shows the household timeline).
    // Returns the actual count (≤ HISTORY_CAPACITY).
    size_t copyRecentEvents(std::array<HistoryEntry, HISTORY_CAPACITY>& out) const;

private:
    void appendHistory(int64_t ts, const char* type, const char* by, uint8_t cat);

    feedme::ports::IClock&        clock_;
    feedme::ports::INetwork&      network_;
    feedme::ports::IStorage&      storage_;
    feedme::domain::CatRoster&    roster_;

    // Per-cat state array, indexed by roster slot. Sized to MAX_CATS so
    // adding a cat at runtime doesn't require resizing.
    std::array<feedme::domain::FeedingState,
               feedme::domain::CatRoster::MAX_CATS> states_{};
    std::array<int64_t,
               feedme::domain::CatRoster::MAX_CATS> justFedClearAt_{};
    int64_t lastNetworkPollSec_ = 0;
    bool    wasOnline_          = false;  // for offline→online edge detection

    // Ring buffer of the last HISTORY_CAPACITY events. `historyHead_`
    // points at the slot the *next* event will go into; `historyCount_`
    // saturates at HISTORY_CAPACITY. Newest-first iteration walks
    // backwards from head. Global across cats; per-cat filtering is a
    // future option if a per-cat-history view lands.
    std::array<HistoryEntry, HISTORY_CAPACITY> history_{};
    size_t historyHead_  = 0;
    size_t historyCount_ = 0;
};

}  // namespace feedme::application
