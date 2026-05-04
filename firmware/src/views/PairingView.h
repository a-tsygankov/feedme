#pragma once

#include "views/IView.h"

namespace feedme::views {

// One-time pairing screen. Shown after first-boot Wi-Fi setup (or any
// time the user resets pairing) until the user dismisses it with a tap.
//
// Renders a QR code containing the webapp setup URL —
//   https://feedme-webapp.pages.dev/setup?hid=feedme-abcdef
// — plus the hid as plain text underneath, in case the user can't /
// won't scan and prefers to type it on a desktop.
//
// Dismissal is a single tap or knob press. The dismissal fires the
// onPaired_ callback (wired in main.cpp to set the NVS paired flag),
// then transitions to "idle". Long-press / long-touch fall through to
// ScreenManager's universal "back to parent" — also "idle" here, so
// the universal gesture is equivalent to dismissal.
class PairingView : public IView {
public:
    // Called when the user taps to dismiss. main.cpp wires this to
    // prefs.setPaired(true) so the screen doesn't re-appear next boot.
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

private:
    const char* hid_       = "";
    const char* url_       = "";
    void (*onPaired_)()    = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* qrcode_   = nullptr;
    lv_obj_t* hidLbl_   = nullptr;
    lv_obj_t* hintLbl_  = nullptr;
};

}  // namespace feedme::views
