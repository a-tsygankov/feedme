#pragma once

#include "ports/INetwork.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.4 — Wi-Fi reset confirmation.
//
// Today's Wi-Fi credentials live in wifi_credentials.h (build flag),
// not NVS, so there are no stored creds to clear yet. The view
// already exists end-to-end so that when the captive portal lands
// (roadmap Phase 2.4) and starts persisting SSID/password to NVS, the
// reset path is just one callback wire-up away. For now Press triggers
// a plain reboot via the supplied callback.
//
// Gestures:
//   Press / Tap          → confirm reset (callback) — typically reboots
//   RotateCW / RotateCCW → cancel back to Settings (user changed mind)
//   LongPress / LongTouch → routed to LockConfirm by the dispatcher;
//                           release returns to this view, no reset.
class WifiResetView : public IView {
public:
    using ResetCallback = void (*)();

    void setOnConfirm(ResetCallback cb) { onConfirm_ = cb; }
    // Read-only access to current connection state — surfaced as
    // SSID / IP / RSSI lines on the confirmation screen so the user
    // sees what they're switching FROM before confirming.
    void setNetwork(const feedme::ports::INetwork* net) { network_ = net; }

    const char* name()   const override { return "wifiReset"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    ResetCallback onConfirm_ = nullptr;
    const feedme::ports::INetwork* network_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* iconLbl_   = nullptr;
    lv_obj_t* titleLbl_  = nullptr;
    lv_obj_t* ssidLbl_   = nullptr;   // current network name
    lv_obj_t* ipLbl_     = nullptr;   // current IP
    lv_obj_t* rssiLbl_   = nullptr;   // signal strength as bars + dBm
    lv_obj_t* bodyLbl_   = nullptr;
    lv_obj_t* hint_      = nullptr;
};

}  // namespace feedme::views
