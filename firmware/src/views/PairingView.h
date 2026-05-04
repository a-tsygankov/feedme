#pragma once

#include "views/IView.h"

#include <stdint.h>

namespace feedme::application { class SyncService; }
namespace feedme::ports       { class IPreferences; }

namespace feedme::views {

// First-boot (and post-Reset) pairing screen.
//
// Renders a QR encoding the webapp setup URL —
//   https://feedme-webapp.pages.dev/setup?hid=feedme-abcdef
// — plus the hid below it for users who can't / won't scan.
//
// AUTO-PAIR (post-PR #34):
//   The view OWNS the pair-handshake polling loop. On entry it calls
//   /api/pair/start (opens the 3-min server-side window) and starts
//   polling /api/pair/check every 15 s. When the webapp's auth flow
//   (POST /api/auth/setup or /login or /quick-setup, with the
//   deviceId carried through) lands, the backend marks the row
//   confirmed + writes the device_token; the next /pair/check picks
//   it up and the view transitions to "syncing" automatically — no
//   tap on the device required.
//
//   Before this fix the view was a passive QR display: /pair/start
//   only fired when the user TAPPED the device (transitioning to a
//   separate PairingProgressView). Nobody scans a QR and then thinks
//   to also tap the source device, so the typical "scan + sign in"
//   flow ended with the device frozen on the QR forever.
//
// Long-press / long-touch goes to ResetPairConfirmView (the "I want a
// fresh start" gesture; rotates the hid + reboots).
class PairingView : public IView {
public:
    // Wire SyncService + IPreferences from main.cpp at boot. Without
    // these the view degrades to a static QR display (no polling).
    void setSyncService(feedme::application::SyncService* svc) { svc_ = svc; }
    void setPreferences(feedme::ports::IPreferences* prefs)    { prefs_ = prefs; }

    // Called once when the device first transitions into pairing
    // success. main.cpp uses this to flip the NVS paired flag so
    // the screen doesn't re-appear on the next boot.
    void setOnPaired(void (*cb)()) { onPaired_ = cb; }

    // Set the household identifier and the URL to encode in the QR.
    // Both are pointers to caller-owned strings (main.cpp keeps them
    // alive in static buffers).
    void setHid(const char* hid) { hid_ = hid ? hid : ""; }
    void setUrl(const char* url) { url_ = url ? url : ""; }

    const char* name()   const override { return "pairing"; }
    const char* parent() const override { return "idle"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    const char* hid_       = "";
    const char* url_       = "";
    void (*onPaired_)()    = nullptr;

    feedme::application::SyncService* svc_   = nullptr;
    feedme::ports::IPreferences*      prefs_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* titleLbl_  = nullptr;
    lv_obj_t* qrcode_    = nullptr;
    lv_obj_t* hidLbl_    = nullptr;
    lv_obj_t* hintLbl_   = nullptr;
    lv_obj_t* statusLbl_ = nullptr;       // bottom status: "waiting…" / "polling" / "tap to retry"

    // Polling state (millis()-based).
    enum class Phase {
        Starting,        // pair/start hasn't succeeded yet — keep retrying every START_RETRY_MS
        Polling,         // pair/start ok; pair/check on POLL_INTERVAL_MS cadence
        ForcePollNext,   // tap requested an out-of-band immediate poll
    };
    Phase     phase_      = Phase::Starting;
    uint32_t  enteredMs_  = 0;
    uint32_t  lastTryMs_  = 0;            // last time we called pair/start OR pair/check
    int       startAttempts_ = 0;         // for status display ("retry #3")
    const char* terminal_ = nullptr;      // non-null = next view to transition to
    // Title-label flash state — render() restores the default title
    // ("Scan to pair") once millis() crosses titleFlashUntilMs_.
    uint32_t  titleFlashUntilMs_ = 0;
    static constexpr uint32_t TITLE_FLASH_MS = 800;

    // Helpers — keep render() readable.
    void setStatus(const char* msg, bool error = false);
    bool tryPairStart();   // calls svc_->pairStart, updates phase_ + status

    // pair/start retry happens fast — WiFi might still be connecting on
    // boot, but once it's up we want to converge in a couple seconds.
    static constexpr uint32_t START_RETRY_MS  = 3000;
    // pair/check polling once paired window is open — server-side rows
    // last 3 min so 15 s gives at most 11 polls per window.
    static constexpr uint32_t POLL_INTERVAL_MS = 15000;
};

}  // namespace feedme::views
