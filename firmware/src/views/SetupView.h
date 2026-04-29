#pragma once

#include "views/IView.h"

namespace feedme::views {

// Captive-portal setup screen. Shown when the device boots without
// stored Wi-Fi credentials (and no build-flag fallback). Pure text:
// the AP name + URL the user should connect to from a phone.
//
// Doesn't accept input — interaction happens in the browser. The
// dispatcher in main.cpp doesn't even pump events while in setup
// mode (only the captive portal's loop runs).
class SetupView : public IView {
public:
    void setApName(const char* apName) { apName_ = apName ? apName : "feedme-?"; }
    void setUrl   (const char* url)    { url_    = url    ? url    : "192.168.4.1"; }

    const char* name()   const override { return "setup"; }
    const char* parent() const override { return "setup"; }  // root in this mode
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    const char* apName_ = "feedme-?";
    const char* url_    = "192.168.4.1";

    lv_obj_t* root_       = nullptr;
    lv_obj_t* titleLbl_   = nullptr;
    lv_obj_t* line1Lbl_   = nullptr;  // "connect to"
    lv_obj_t* apLbl_      = nullptr;  // ap name in accent
    lv_obj_t* line3Lbl_   = nullptr;  // "then open"
    lv_obj_t* urlLbl_     = nullptr;  // url in accent
};

}  // namespace feedme::views
