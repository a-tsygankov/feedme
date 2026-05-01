#pragma once

#include "views/IView.h"

namespace feedme::views {

// "Reset pairing" confirmation. Reached from PairingView via long-press
// (or from Settings → Pair → long-press once we get there). Confirms
// the destructive action then fires the onConfirm callback wired in
// main.cpp, which:
//   1. Increments the NVS hid-reset counter
//   2. Clears the stored hid (next boot regenerates feedme-{mac6}-{n+1})
//   3. Clears the paired flag (PairingView re-shows on next boot)
//   4. Reboots the device
//
// The OLD backend household record stays orphaned — the user can't
// recover it without proof of physical access (which is what the
// reset gesture itself constitutes). They start fresh with a new hid.
//
// Cancel = any rotate or long-press (back to pairing).
class ResetPairConfirmView : public IView {
public:
    void setOnConfirm(void (*cb)()) { onConfirm_ = cb; }

    const char* name()   const override { return "resetPairConfirm"; }
    const char* parent() const override { return "pairing"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void (*onConfirm_)() = nullptr;

    lv_obj_t* root_     = nullptr;
    lv_obj_t* iconLbl_  = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* bodyLbl_  = nullptr;
    lv_obj_t* hintLbl_  = nullptr;
};

}  // namespace feedme::views
