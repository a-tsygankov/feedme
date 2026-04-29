#pragma once

#include "ports/INetwork.h"

#include <string>

namespace feedme::adapters {

// HTTPS network adapter — Phase 2.1. Talks to the Cloudflare Worker
// in backend/ over the existing 3 endpoints:
//   GET  /api/state?hid=<hid>            → FeedingState snapshot
//   POST /api/feed   {hid, by, type?}    → log a feed/snooze event
//   GET  /api/history?hid=<hid>&n=N      → recent events (not used by
//                                          the firmware today; the
//                                          local LittleFS history wins)
//
// Single-cat for v0 — matches the backend schema as it ships. Per-cat
// extension lands when both INetwork and the schema evolve to carry
// Cat::id; until then the device's per-cat state is local-only beyond
// slot 0 ("primary cat") which is the one the worker syncs.
//
// HTTPS uses WiFiClientSecure with setInsecure() — no cert pinning in
// v0. The Worker is on Cloudflare which presents a valid public cert,
// so a future setCACert() call upgrades this to verified TLS without
// API changes.
//
// Configuration via build flags (platformio.ini):
//   -DFEEDME_BACKEND_URL="https://feedme.<account>.workers.dev"
//   -DFEEDME_HID="home-4a7f"
// Both empty → behaves like NoopNetwork (returns nullopt, isOnline
// reflects raw Wi-Fi link state). The composition root in main.cpp
// picks WifiNetwork only when both are set.
class WifiNetwork : public feedme::ports::INetwork {
public:
    WifiNetwork(const char* baseUrl, const char* hid);

    // Replace hid at runtime — used after captive-portal setup or
    // NVS load when the build-flag value is overridden by stored
    // creds. Empty string disables fetch/post (treats as offline).
    void setHid(const char* hid);

    void begin() override;
    bool isOnline() const override;
    std::optional<feedme::domain::FeedingState>
    fetchState(uint8_t catId) override;
    bool postFeed(const std::string& by, int64_t ts, uint8_t catId) override;
    bool postSnooze(const std::string& by, int64_t ts, int durationSec,
                    uint8_t catId) override;

private:
    bool postEvent(const std::string& by, const char* type, int durationSec,
                   uint8_t catId);

    std::string baseUrl_;   // no trailing slash
    std::string hid_;
};

}  // namespace feedme::adapters
