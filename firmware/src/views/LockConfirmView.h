#pragma once

#include "ports/ITapSensor.h"
#include "views/IView.h"

namespace feedme::views {

// 12 Lock Confirm — the cross-cutting parental gate.
//
// Per FeedMeKnob FSM: long-press / long-touch from any screen (except
// Pouring, which uses long-touch as a direct cancel) opens this view.
// The accent ring fills as the user keeps holding. Two outcomes:
//   - Hold to completion (HOLD_DURATION_MS) → destructive action runs
//     and the next view is "idle" (per the FSM's join arrow).
//   - Release before completion → cancel, next view is whatever the
//     dispatcher captured as `returnTo_` before transitioning here.
//
// Release detection: the discrete TapEvent stream gives no signal for
// "press ended", so the view polls each sensor's isPressed() each
// render call. Held = touch.isPressed() OR button.isPressed().
//
// "Destructive action" for v0 is a no-op + Serial log ("clear today's
// schedule") matching the placeholder in feedmeknob-plan.md C.5.
class LockConfirmView : public IView {
public:
    static constexpr uint32_t HOLD_DURATION_MS = 1500;

    void setSensors(const feedme::ports::ITapSensor* touch,
                    const feedme::ports::ITapSensor* button) {
        touch_  = touch;
        button_ = button;
    }
    // Called by the dispatcher before transition() so the view knows
    // where to bounce back on early release. Pointer must be a stable
    // string literal (i.e. another view's name()).
    void setReturnTo(const char* viewName) { returnTo_ = viewName; }

    const char* name() const override { return "lockConfirm"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;
    const char* nextView() override;

private:
    bool stillHeld() const;

    const feedme::ports::ITapSensor* touch_  = nullptr;
    const feedme::ports::ITapSensor* button_ = nullptr;
    const char* returnTo_ = "idle";

    lv_obj_t* root_      = nullptr;
    lv_obj_t* dialArc_   = nullptr;
    lv_obj_t* fillArc_   = nullptr;
    lv_obj_t* iconLbl_   = nullptr;
    lv_obj_t* titleLbl_  = nullptr;
    lv_obj_t* hintLbl_   = nullptr;

    uint32_t startMs_   = 0;
    bool     completed_ = false;
    bool     released_  = false;
    int      lastSweep_ = -1;
};

}  // namespace feedme::views
