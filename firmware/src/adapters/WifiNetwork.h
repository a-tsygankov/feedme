#pragma once

#include "domain/TimeZone.h"
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

    // Optional TimeZone reference. When set, fetchState appends
    // `&tzOffset=N` to the GET URL so the backend rolls "today" over
    // at local midnight instead of UTC. Read live so changes via the
    // Settings → Timezone editor take effect on the next poll
    // without a re-flash. Null → omit the param (backend defaults
    // to UTC).
    void setTimeZone(const feedme::domain::TimeZone* tz) { tz_ = tz; }

    void begin() override;
    bool isOnline() const override;
    std::optional<feedme::domain::FeedingState>
    fetchState(uint8_t catId) override;
    bool postFeed(const std::string& by, int64_t ts, uint8_t catId,
                  const std::string& eventId) override;
    bool postSnooze(const std::string& by, int64_t ts, int durationSec,
                    uint8_t catId,
                    const std::string& eventId) override;

    // Connection inspection — empty / 0 if Wi-Fi isn't associated.
    std::string ssid()      const override;
    int         rssi()      const override;
    std::string ipAddress() const override;

    // ── Phase C — generic JSON HTTP for sync + pair lifecycle ─────
    //
    // The pair endpoints (start/check/cancel) and /api/sync need
    // arbitrary JSON request/response bodies that don't fit the
    // existing fixed-shape postFeed / fetchState helpers. This thin
    // generic layer lets SyncService own the JSON shape entirely:
    // it builds the request body, calls one of these, and parses
    // the response.
    //
    // bearerToken is the Authorization: Bearer header value. Pass
    // an empty string for unauthenticated endpoints (pair lifecycle).
    //
    // status == 0 indicates a network-level failure (no DNS / no
    // route / connect timeout). Non-zero is whatever the Worker
    // returned. body holds the raw response text — caller parses.
    struct HttpResult {
        int         status = 0;
        std::string body;
    };
    HttpResult httpPostJson(const char* path,
                            const std::string& jsonBody,
                            const std::string& bearerToken = "");
    HttpResult httpGet(const char* path,
                       const std::string& bearerToken = "");
    HttpResult httpDelete(const char* path,
                          const std::string& bearerToken = "");

private:
    bool postEvent(const std::string& by, const char* type, int durationSec,
                   uint8_t catId, const std::string& eventId);

    std::string baseUrl_;   // no trailing slash
    std::string hid_;
    const feedme::domain::TimeZone* tz_ = nullptr;
};

}  // namespace feedme::adapters
