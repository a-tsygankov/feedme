#pragma once

#include "domain/FeedingState.h"
#include "ports/IClock.h"
#include "ports/INetwork.h"
#include "ports/IStorage.h"

#include <array>
#include <string>

namespace feedme::application {

// Owns the canonical FeedingState for this device. Mediates between user
// input, the clock, the network, and offline storage. Knows nothing about
// the display — DisplayCoordinator subscribes to it.
class FeedingService {
public:
    static constexpr size_t HISTORY_CAPACITY = 5;

    struct HistoryEntry {
        int64_t     ts = 0;        // unix seconds
        std::string type;          // "feed" | "snooze"
        std::string by;
    };

    FeedingService(feedme::ports::IClock& clock,
                   feedme::ports::INetwork& network,
                   feedme::ports::IStorage& storage);

    // Idempotent. Pulls latest from network if available; otherwise advances
    // the local "justFed" flag etc. Call from loop() at ~1 Hz.
    void tick();

    // Tap actions. Each returns the new local state immediately and queues
    // the event for replay if offline.
    void logFeeding(const char* by);
    void snooze(const char* by, int durationSec);

    // Pull persisted history out of storage into the in-memory ring
    // buffer. Call once after storage.begin() so the history view shows
    // entries from previous boots.
    void loadHistoryFromStorage();

    const feedme::domain::FeedingState& state() const { return state_; }

    // Most-recent-first iteration over the in-memory history ring buffer.
    // Returns the actual count (≤ HISTORY_CAPACITY).
    size_t copyRecentEvents(std::array<HistoryEntry, HISTORY_CAPACITY>& out) const;

private:
    void appendHistory(int64_t ts, const char* type, const char* by);

    feedme::ports::IClock&   clock_;
    feedme::ports::INetwork& network_;
    feedme::ports::IStorage& storage_;
    feedme::domain::FeedingState state_;
    int64_t justFedClearAt_     = 0;
    int64_t lastNetworkPollSec_ = 0;

    // Ring buffer of the last HISTORY_CAPACITY events. `historyHead_` points
    // at the slot the *next* event will go into; `historyCount_` saturates
    // at HISTORY_CAPACITY. Newest-first iteration walks backwards from head.
    std::array<HistoryEntry, HISTORY_CAPACITY> history_{};
    size_t historyHead_  = 0;
    size_t historyCount_ = 0;
};

}  // namespace feedme::application
