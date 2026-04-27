#pragma once

#include "domain/FeedingState.h"
#include <cstdint>
#include <optional>
#include <string>

namespace feedme::ports {

class INetwork {
public:
    virtual ~INetwork() = default;
    virtual void begin() = 0;
    virtual bool isOnline() const = 0;

    // Pull current state from the backend. nullopt on failure.
    virtual std::optional<feedme::domain::FeedingState> fetchState() = 0;

    // Push a feed event. Returns the new state if the server returned one.
    virtual std::optional<feedme::domain::FeedingState>
    postFeed(const std::string& by, int64_t ts) = 0;

    virtual std::optional<feedme::domain::FeedingState>
    postSnooze(const std::string& by, int64_t ts, int durationSec) = 0;
};

}  // namespace feedme::ports
