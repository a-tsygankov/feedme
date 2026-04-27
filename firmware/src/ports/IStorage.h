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
    virtual void enqueue(const PendingEvent& e) = 0;
    virtual std::vector<PendingEvent> drainPending() = 0;
};

}  // namespace feedme::ports
