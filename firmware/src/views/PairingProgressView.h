#pragma once

#include "views/IView.h"

namespace feedme::application { class SyncService; }
namespace feedme::ports       { class IPreferences; }

namespace feedme::views {

// Pairing progress screen. Shown after the user taps the QR-display
// (PairingView) to "continue pairing". Drives the device side of the
// 3-min handshake described in docs/sync-implementation-handoff.md §7.
//
// Lifecycle:
//   onEnter   — POST /api/pair/start once, then animated dots + 15-s
//               poll loop on /api/pair/check.
//   render    — pumps the poll timer; on Confirmed, persists token +
//               home name to NVS, sets the paired flag, and transitions
//               to "syncing" for the initial sync.
//   handleInput LongTap/LongPress — POST /api/pair/cancel + back to "idle".
//   nextView  — returns the chosen destination once a terminal state
//               (Confirmed / Expired / Cancelled / NetworkError) lands.
//
// Cancellation semantics: a long-tap during the 5-second HTTP timeout
// of an in-flight /pair/check WILL be processed at the start of the
// next poll cycle, not mid-call. Worst-case wait after a cancel tap
// is 5 s. Documented in the sync handoff doc.
class PairingProgressView : public IView {
public:
    void setSyncService(feedme::application::SyncService* svc) { svc_ = svc; }
    void setPreferences(feedme::ports::IPreferences* prefs)    { prefs_ = prefs; }

    const char* name()   const override { return "pairingProgress"; }
    const char* parent() const override { return "idle"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    feedme::application::SyncService*  svc_   = nullptr;
    feedme::ports::IPreferences*       prefs_ = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* dotsLbl_  = nullptr;
    lv_obj_t* hintLbl_  = nullptr;

    // Timer state — all millis() since enter.
    uint32_t  enteredMs_   = 0;
    uint32_t  lastPollMs_  = 0;
    uint32_t  lastDotsMs_  = 0;
    int       dotsFrame_   = 0;
    bool      startedOk_   = false;
    const char* terminal_  = nullptr;   // non-null = transition destination

    static constexpr uint32_t POLL_INTERVAL_MS = 15000;
    static constexpr uint32_t DOTS_INTERVAL_MS = 350;
    static constexpr uint32_t TIMEOUT_MS       = 180000;   // 3 min
};

}  // namespace feedme::views
