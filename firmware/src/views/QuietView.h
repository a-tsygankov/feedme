#pragma once

#include "domain/QuietWindow.h"
#include "views/IView.h"

namespace feedme::views {

// 08 Quiet hours — 24-h dial with the quiet wedge highlighted, a
// "now" tick at the current minute, and a centre title + range label.
//
// Per FeedMeKnob handoff §3:
//   touch: tap → toggle on/off (persisted)
//   knob:  read-only — tap/press also toggles (alt path)
//   long-press / long-touch → menu (canonical cancel)
//
// Wedge tinting: full accent when enabled, faint when disabled, so
// the schedule shape is visible either way.
class QuietView : public IView {
public:
    void setQuiet(feedme::domain::QuietWindow* quiet) { quiet_ = quiet; }

    const char* name()   const override { return "quiet"; }
    const char* parent() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw(int hour, int minute);
    void applyEnabledStyles();
    void applyTimeStyles();
    void positionNowTick(int hour, int minute);

    feedme::domain::QuietWindow* quiet_ = nullptr;

    lv_obj_t* root_         = nullptr;
    lv_obj_t* dialArc_      = nullptr;
    lv_obj_t* quietArc_     = nullptr;
    lv_obj_t* nowTick_      = nullptr;
    lv_obj_t* titleLbl_     = nullptr;
    lv_obj_t* rangeLbl_     = nullptr;
    lv_obj_t* statusLbl_    = nullptr;

    int  lastDrawnHour_   = -1;
    int  lastDrawnMinute_ = -1;
    bool lastDrawnEnabled_ = false;
    int  lastDrawnStartHour_   = -1;
    int  lastDrawnStartMinute_ = -1;
    int  lastDrawnEndHour_     = -1;
    int  lastDrawnEndMinute_   = -1;
    bool firstRender_     = true;
};

}  // namespace feedme::views
