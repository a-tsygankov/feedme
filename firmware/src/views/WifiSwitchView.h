#pragma once

#include "adapters/WifiCaptivePortal.h"
#include "views/IView.h"

namespace feedme::views {

// Phase 2.4 follow-up — In-place Wi-Fi switch status screen.
//
// Reached from WifiResetView ("Switch Wi-Fi") after the user confirms.
// Sister to SetupView (the boot-time setup screen) but watches a live
// WifiCaptivePortal state machine running in AP+STA mode — STA stays
// up on the existing network until the user submits new creds via the
// SoftAP form.
//
// What the user sees, by portal state:
//   Advertising — "Connect to feedme-XXXX" / "192.168.4.1"
//   Switching   — "Connecting to <new ssid>..."
//   Done        — "Connected" briefly, then auto-return to Settings
//   Failed      — "Connect failed" + retry hint; long-press cancels
//
// Doesn't accept knob/tap selection inputs — the user interacts via
// the phone web form. Long-press is the cancel/back gesture (handled
// by ScreenManager fallback to parent="settings"). When the view
// leaves it stops the portal so the AP MAC is dropped and STA is
// left whatever it became (new network on Done, original network on
// Failed/cancel).
class WifiSwitchView : public IView {
public:
    void setPortal(feedme::adapters::WifiCaptivePortal* portal) { portal_ = portal; }

    const char* name()   const override { return "wifiSwitch"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;   // auto-return to settings after Done

private:
    feedme::adapters::WifiCaptivePortal* portal_ = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* line1Lbl_ = nullptr;   // "connect to" / "connecting to"
    lv_obj_t* primaryLbl_ = nullptr; // ap name OR target ssid
    lv_obj_t* line3Lbl_ = nullptr;   // "then open" / status
    lv_obj_t* secondaryLbl_ = nullptr; // url OR error / done
    lv_obj_t* hintLbl_  = nullptr;

    int      lastDrawnState_ = -1;   // cast of WifiCaptivePortal::State
    uint32_t doneEnteredMs_  = 0;    // when we first hit Done
    bool     pendingExit_    = false;
};

}  // namespace feedme::views
