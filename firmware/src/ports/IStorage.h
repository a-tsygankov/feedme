#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace feedme::ports {

struct PendingEvent {
    std::string clientEventId;  // UUID for idempotency
    int64_t     ts;
    std::string type;            // "feed" | "snooze"
    std::string by;
};

class IStorage {
public:
    virtual ~IStorage() = default;
    virtual void begin() = 0;

    // Offline event queue — events that haven't been confirmed sent to
    // the backend. enqueue/drainPending pair: drain returns everything
    // and clears the queue.
    virtual void enqueue(const PendingEvent& e) = 0;
    virtual std::vector<PendingEvent> drainPending() = 0;

    // Persistent feed/snooze log used by the on-device history view.
    // Append-only; loadRecentHistory returns the last `n` records
    // newest-first.
    virtual void recordHistory(const PendingEvent& e) = 0;
    virtual std::vector<PendingEvent> loadRecentHistory(size_t n) = 0;
};

}  // namespace feedme::ports
