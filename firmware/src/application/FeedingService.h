#pragma once

#include "domain/FeedingState.h"
#include "ports/IClock.h"
#include "ports/INetwork.h"
#include "ports/IStorage.h"

namespace feedme::application {

// Owns the canonical FeedingState for this device. Mediates between user
// input, the clock, the network, and offline storage. Knows nothing about
// the display — DisplayCoordinator subscribes to it.
class FeedingService {
public:
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

    const feedme::domain::FeedingState& state() const { return state_; }

private:
    feedme::ports::IClock&   clock_;
    feedme::ports::INetwork& network_;
    feedme::ports::IStorage& storage_;
    feedme::domain::FeedingState state_;
    int64_t justFedClearAt_ = 0;
    int64_t lastNetworkPollSec_ = 0;
};

}  // namespace feedme::application
