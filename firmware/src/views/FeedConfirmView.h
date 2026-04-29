#pragma once

#include "domain/CatRoster.h"
#include "domain/UserRoster.h"
#include "views/IView.h"

namespace feedme::views {

// 04 Feed Confirm — hungry cat hero, 270° portion arc, "X g".
// Per FeedMeKnob handoff §3:
//   knob:  rotate ±5 g, press → pour
//   touch: tap → portion adjust, long-tap → cancel back to menu
class FeedConfirmView : public IView {
public:
    // Roster carries the per-cat portion used by this screen — the
    // active cat's portion is read each render via activePortion().
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }
    // Optional: when set and count() >= 2, Press routes to the
    // "by whom?" picker (feederPick) instead of straight to Pouring.
    // Adaptive UI rule — N=1 households never see the picker.
    void setUserRoster(const feedme::domain::UserRoster* users) { users_ = users; }

    const char* name() const override { return "feedConfirm"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::domain::CatRoster*        roster_ = nullptr;
    const feedme::domain::UserRoster* users_  = nullptr;

    lv_obj_t* root_         = nullptr;
    lv_obj_t* arcBg_        = nullptr;
    lv_obj_t* arcFg_        = nullptr;
    lv_obj_t* catImg_       = nullptr;
    lv_obj_t* portionLbl_   = nullptr;
    lv_obj_t* unitLbl_      = nullptr;
    lv_obj_t* hintLbl_      = nullptr;
    int       lastDrawnG_   = -1;
};

}  // namespace feedme::views
