#pragma once

#include "ports/INetwork.h"

namespace feedme::adapters {

// Offline-only network adapter; lets us run the firmware without Wi-Fi.
class NoopNetwork : public feedme::ports::INetwork {
public:
    void begin() override {}
    bool isOnline() const override { return false; }
    std::optional<feedme::domain::FeedingState> fetchState() override {
        return std::nullopt;
    }
    std::optional<feedme::domain::FeedingState>
    postFeed(const std::string&, int64_t) override { return std::nullopt; }
    std::optional<feedme::domain::FeedingState>
    postSnooze(const std::string&, int64_t, int) override { return std::nullopt; }
};

}  // namespace feedme::adapters
