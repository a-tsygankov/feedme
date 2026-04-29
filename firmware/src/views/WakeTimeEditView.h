#pragma once

#include "domain/WakeTime.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.1 — Wake-time editor.
//
// Two fields, hour and minute. Pattern (per FeedMeKnob §3 conventions):
//   Rotate CW / CCW  → adjust the focused field (HH 0..23, MM 5-min steps)
//   Press / Tap      → confirm focus, advance hour → minute → save → settings
//
// Long-press is shadowed by the C.5 LockConfirm interception
// (releasing brings you back here without saving), so editors don't
// own that gesture. There's no in-editor "abandon" — to revert, rotate
// back to the original value before the final Press, or re-enter the
// editor.
class WakeTimeEditView : public IView {
public:
    void setWakeTime(feedme::domain::WakeTime* wake) { wake_ = wake; }

    const char* name()   const override { return "wakeTimeEdit"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    enum class Field { Hour, Minute };

    void redraw();
    void applyFocusStyles();

    feedme::domain::WakeTime* wake_ = nullptr;

    lv_obj_t* root_       = nullptr;
    lv_obj_t* hourLbl_    = nullptr;
    lv_obj_t* colonLbl_   = nullptr;
    lv_obj_t* minuteLbl_  = nullptr;
    lv_obj_t* fieldLbl_   = nullptr;
    lv_obj_t* hintLbl_    = nullptr;

    Field focus_           = Field::Hour;
    int   lastDrawnHour_   = -1;
    int   lastDrawnMinute_ = -1;
    Field lastDrawnFocus_  = Field::Hour;
    bool  firstRender_     = true;
};

}  // namespace feedme::views
