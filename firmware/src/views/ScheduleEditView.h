#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Schedule editor — cycles through the 4 meal slots one at a time;
// rotate adjusts the focused slot's hour, press advances. Same UX
// pattern as WakeTimeEditView / QuietHoursEditView.
//
// Reached from ScheduleView.Press (the gesture canon left Press
// unclaimed for Schedule). Final press saves and returns to the
// schedule view; mutations are already applied through
// CatRoster::bumpActiveSlotHour, persistence runs on the next
// service tick via the roster's dirty-flag aggregation.
//
// Edits the **active cat's** schedule — when N>=2 cats are
// configured, switch active via Idle's rotate before entering.
class ScheduleEditView : public IView {
public:
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }

    const char* name()   const override { return "scheduleEdit"; }
    const char* parent() const override { return "schedule"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::domain::CatRoster* roster_ = nullptr;

    lv_obj_t* root_       = nullptr;
    lv_obj_t* headerLbl_  = nullptr;     // "MEAL TIMES"
    lv_obj_t* slotLbl_    = nullptr;     // "Breakfast" / "Lunch" / ...
    lv_obj_t* hourLbl_    = nullptr;     // big "07"
    lv_obj_t* unitLbl_    = nullptr;     // ":00"
    lv_obj_t* hintLbl_    = nullptr;

    int  focusSlot_       = 0;
    int  lastDrawnSlot_   = -1;
    int  lastDrawnHour_   = -1;
    bool firstRender_     = true;
};

}  // namespace feedme::views
