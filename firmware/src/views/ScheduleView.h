#pragma once

#include "domain/CatRoster.h"
#include "domain/MealSchedule.h"
#include "views/IView.h"

namespace feedme::views {

// 07 Schedule — 4 slot circles at 12/3/6/9 o'clock around the perimeter.
// Per FeedMeKnob handoff §3:
//   knob:  rotate to next slot (scrub the inspected slot)
//   touch: tap → idle
//   long-touch / long-press → menu (canonical cancel)
//
// "Served" is wall-clock-based for C.2 — the slot whose hour has passed
// is filled accent. The "now" border tracks the actual current slot,
// independent of the user's scrub focus.
class ScheduleView : public IView {
public:
    // Roster is held instead of a single MealSchedule pointer so the
    // view always reads the active cat's schedule — important once
    // a cat-selector lands and switches active at runtime.
    void setRoster(const feedme::domain::CatRoster* roster) {
        roster_ = roster;
    }

    const char* name() const override { return "schedule"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw(int hour);
    void applySlotStyles(int currentIdx);

    const feedme::domain::CatRoster*    roster_   = nullptr;

    lv_obj_t* root_                    = nullptr;
    lv_obj_t* centreTime_              = nullptr;
    lv_obj_t* centreLabel_             = nullptr;
    lv_obj_t* slotCircles_[feedme::domain::MealSchedule::SLOT_COUNT] = {nullptr};
    lv_obj_t* slotTimes_  [feedme::domain::MealSchedule::SLOT_COUNT] = {nullptr};
    lv_obj_t* slotSubs_   [feedme::domain::MealSchedule::SLOT_COUNT] = {nullptr};

    int  selectedIdx_ = -1;   // -1 = "default to current on next render"
    int  lastDrawnHour_     = -1;
    int  lastDrawnSelected_ = -1;
};

}  // namespace feedme::views
