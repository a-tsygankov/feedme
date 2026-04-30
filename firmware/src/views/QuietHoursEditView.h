#pragma once

#include "domain/QuietWindow.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.2 — Quiet hours start/end editor.
//
// Four fields cycled in order: start-hour, start-minute, end-hour,
// end-minute. Same gesture pattern as WakeTimeEditView.
//
//   Rotate CW / CCW  → adjust focused field (HH 0..23, MM 5-min steps)
//   Press / Tap      → confirm focus, advance → final press saves and
//                      returns to Settings.
//
// The wedge geometry on QuietView re-renders automatically next time
// the user navigates back to it — QuietView::redraw watches start/end.
class QuietHoursEditView : public IView {
public:
    void setQuiet(feedme::domain::QuietWindow* quiet) { quiet_ = quiet; }

    const char* name()   const override { return "quietHoursEdit"; }
    const char* parent() const override { return "quiet"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    enum class Field { StartHour, StartMinute, EndHour, EndMinute };

    void redraw();
    void applyFocusStyles();

    feedme::domain::QuietWindow* quiet_ = nullptr;

    lv_obj_t* root_         = nullptr;
    lv_obj_t* fieldLbl_     = nullptr;     // "QUIET START" / "QUIET END"
    lv_obj_t* hourLbl_      = nullptr;
    lv_obj_t* colonLbl_     = nullptr;
    lv_obj_t* minuteLbl_    = nullptr;
    lv_obj_t* arrowLbl_     = nullptr;     // small "→" between two HH:MM
    lv_obj_t* hint_         = nullptr;

    Field focus_           = Field::StartHour;
    int   lastDrawnSH_     = -1;
    int   lastDrawnSM_     = -1;
    int   lastDrawnEH_     = -1;
    int   lastDrawnEM_     = -1;
    Field lastDrawnFocus_  = Field::StartHour;
    bool  firstRender_     = true;
};

}  // namespace feedme::views
