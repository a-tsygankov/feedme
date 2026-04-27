#pragma once

#include <cstdint>

namespace feedme::domain {

// Value object — the entire "what does the device know" snapshot.
// Pure data; no Arduino dependencies. Immutable in spirit (we copy, not mutate).
struct FeedingState {
    int64_t lastFeedTs    = 0;  // unix seconds; 0 == never fed
    int64_t snoozeUntilTs = 0;  // unix seconds; 0 == not snoozed
    int     todayCount    = 0;  // feeds since local midnight
    bool    justFed       = false;  // transient: set for ~3s after a tap
};

}  // namespace feedme::domain
