#pragma once

#include "ports/INetwork.h"

#if !defined(SIMULATOR)
#  include <WiFi.h>
#endif

namespace feedme::adapters {

// Network adapter stub. Posts and fetches are no-ops (Phase 2.1 will
// add the real WifiNetwork against the Cloudflare Worker), but
// isOnline() reports the actual Wi-Fi link state on real hardware so
// the Settings → Wi-Fi row reflects reality. In the simulator there's
// no WiFi.h symbol so isOnline() stays false — matches the env, the
// simulator never connects to anything anyway.
class NoopNetwork : public feedme::ports::INetwork {
public:
    void begin() override {}
    bool isOnline() const override {
#if defined(SIMULATOR)
        return false;
#else
        return WiFi.status() == WL_CONNECTED;
#endif
    }
    std::optional<feedme::domain::FeedingState> fetchState() override {
        return std::nullopt;
    }
    std::optional<feedme::domain::FeedingState>
    postFeed(const std::string&, int64_t) override { return std::nullopt; }
    std::optional<feedme::domain::FeedingState>
    postSnooze(const std::string&, int64_t, int) override { return std::nullopt; }
};

}  // namespace feedme::adapters
