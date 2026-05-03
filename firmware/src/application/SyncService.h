#pragma once

#include "domain/CatRoster.h"
#include "domain/UserRoster.h"

#include <stdint.h>
#include <string>

namespace feedme::adapters { class WifiNetwork; }

namespace feedme::application {

// SyncService — application-layer driver for the multi-device sync
// rework. Owns:
//   - the device identity (deviceId + DeviceToken, persisted in NVS
//     by the caller; we hold cached copies)
//   - the cached homeName (from the last pairing confirmation)
//   - lastSyncAt (for the sleep-entry / wake-entry sync gates that
//     land in Phase E)
//
// Three external-facing call sites:
//
//   1. PairingProgressView — drives /api/pair/start once on entry,
//      then polls /api/pair/check every 15s up to a 3-min timeout.
//      A confirmed response stores the device token + home name and
//      transitions to initial-sync.
//
//   2. HomeView Sync menu entry — calls syncFull() directly. Returns
//      true on success; the SyncingView shows running dots while the
//      call is in flight.
//
//   3. ResetPairConfirmView callback — calls unpair() to DELETE the
//      pairing record server-side before the firmware wipes its NVS
//      and reboots into a fresh state.
//
// All methods are SYNCHRONOUS. HTTP timeouts are bounded to 5 s by
// WifiNetwork's HTTPClient configuration; cancellation is post-hoc
// (set the cancel flag, the next /pair/check or syncFull returns
// without applying its result).
//
// The service does not own a state machine field — views track the
// state they care about. We just track the cancel flag.
class SyncService {
public:
    enum class PairResult {
        Pending,         // server says still waiting for webapp confirm
        Confirmed,       // got the token + home name; cached locally
        Expired,         // 3-min window timed out
        Cancelled,       // device or user side cancelled
        NetworkError,    // request failed at HTTP / TLS / DNS level
        UnknownStatus,   // server returned a status we don't recognize
    };

    SyncService(feedme::adapters::WifiNetwork& net,
                feedme::domain::CatRoster& cats,
                feedme::domain::UserRoster& users);

    // Identity setters — main.cpp populates these from NVS at boot
    // and after a successful pairing confirmation.
    void setDeviceId   (const char* id)    { deviceId_    = id ? id : ""; }
    void setDeviceToken(const char* token) { deviceToken_ = token ? token : ""; }
    void setHomeName   (const char* name)  { homeName_    = name ? name : ""; }
    void setLastSyncAt (int64_t ts)        { lastSyncAt_  = ts; }

    const std::string& deviceId()    const { return deviceId_; }
    const std::string& deviceToken() const { return deviceToken_; }
    const std::string& homeName()    const { return homeName_; }
    int64_t            lastSyncAt()  const { return lastSyncAt_; }
    const std::string& lastError()   const { return lastError_; }
    bool               isPaired()    const { return !deviceToken_.empty(); }

    // Cancellation. Set by PairingProgressView / SyncingView on long-tap.
    // The next pairCheck() / syncFull() returns its terminal state
    // without applying any result.
    void requestCancel() { cancelRequested_ = true; }
    bool cancelRequested() const { return cancelRequested_; }
    void clearCancel() { cancelRequested_ = false; }

    // ── Pair lifecycle ───────────────────────────────────────────
    // POST /api/pair/start { deviceId } → opens a 3-min window
    // server-side. Idempotent (server uses INSERT OR REPLACE).
    // Returns false on network failure.
    bool pairStart();

    // GET /api/pair/check?deviceId=… — single-shot, non-blocking
    // beyond the HTTP round-trip. PairingProgressView calls this
    // every 15 s. On Confirmed, deviceToken_ + homeName_ are set
    // and the caller should persist them via NVS, then trigger
    // an initial syncFull().
    PairResult pairCheck();

    // POST /api/pair/cancel { deviceId } — invoked when the user
    // long-taps the Pairing screen. Idempotent; the device's UI can
    // exit immediately regardless of the response.
    bool pairCancel();

    // ── Sync ─────────────────────────────────────────────────────
    // POST /api/sync (DeviceToken) — uploads the device's full
    // roster snapshot, applies the server's merged response back
    // into CatRoster + UserRoster, and bumps lastSyncAt.
    //
    // Returns false on network/parse/auth failure (lastError() then
    // holds the diagnostic). True on 2xx; the rosters have been
    // updated in-place. Caller should mark the rosters dirty +
    // schedule an NVS flush (main.cpp's tick loop already does
    // this via consumeDirty()).
    //
    // Per Phase C scope, this sends ACTIVE cats/users only — no
    // tombstones. Deletions made on this device will not propagate
    // to the server until Phase D introduces UUID-based identity
    // that decouples slot_id reuse from server-side dedup.
    bool syncFull();

    // DELETE /api/pair/<deviceId> (DeviceToken) — invoked by Reset.
    // On success, clears deviceToken_/homeName_ in this service
    // (caller must also wipe NVS + regen deviceId before reboot).
    bool unpair();

private:
    feedme::adapters::WifiNetwork&     net_;
    feedme::domain::CatRoster&         cats_;
    feedme::domain::UserRoster&        users_;
    std::string                        deviceId_;
    std::string                        deviceToken_;
    std::string                        homeName_;
    int64_t                            lastSyncAt_     = 0;
    bool                               cancelRequested_ = false;
    std::string                        lastError_;

    // Build the JSON body for POST /api/sync. Pulled out so it can
    // be unit-tested in isolation (no HTTP needed). Buffer chosen
    // to fit MAX_CATS + MAX_USERS (8 entities × ~250 bytes ≈ 2KB)
    // with comfortable headroom.
    std::string buildSyncRequestBody();
    // Apply the server response (full canonical state) to the
    // rosters. Replaces in-place so existing UI references stay
    // valid. Returns false on bad JSON.
    bool applySyncResponse(const std::string& body);
};

}  // namespace feedme::application
