#pragma once

#include "domain/FeedingState.h"
#include <cstdint>
#include <optional>
#include <string>

namespace feedme::ports {

// All methods are scoped to a single cat (stable Cat::id, not slot
// index — adapters don't need to know about CatRoster). The caller
// (FeedingService) translates slot→id at the boundary so per-cat
// state stays addressable across renames + reorders.
class INetwork {
public:
    virtual ~INetwork() = default;
    virtual void begin() = 0;
    virtual bool isOnline() const = 0;

    // Pull current state for one cat from the backend. Returns
    // nullopt on failure (network down, parse error, non-200).
    virtual std::optional<feedme::domain::FeedingState>
    fetchState(uint8_t catId) = 0;

    // Push events. Return true if the server accepted (HTTP 200);
    // false on any failure (offline, timeout, non-200). The
    // pending-queue drain re-enqueues events whose post returned
    // false. The worker today returns just an ack, so we don't
    // bother bringing back a state — next fetchState picks it up.
    //
    // `eventId` is a per-event UUID used as the backend's idempotency
    // key (Phase 2.x). Events that get re-posted (timeout that the
    // server actually processed → next-online drain replays) carry
    // the same id and the backend silently dedups. Empty string
    // disables the dedup hint.
    virtual bool postFeed(const std::string& by, int64_t ts, uint8_t catId,
                          const std::string& eventId) = 0;

    virtual bool postSnooze(const std::string& by, int64_t ts,
                            int durationSec, uint8_t catId,
                            const std::string& eventId) = 0;

    // ── Connection inspection (read-only diagnostics) ─────────────
    // Used by SettingsView and WifiResetView to surface what we're
    // connected to before the user decides to switch. Defaults
    // return empty / 0 for adapters (NoopNetwork) that don't have a
    // real link. WifiNetwork queries WiFi.SSID()/RSSI()/localIP().
    virtual std::string ssid() const { return {}; }
    virtual int         rssi() const { return 0; }   // dBm; 0 == unknown
    virtual std::string ipAddress() const { return {}; }
};

}  // namespace feedme::ports
