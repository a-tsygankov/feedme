#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// 02 Idle — time + ambient cat + footer hint.
// Per FeedMeKnob handoff: tap or knob press advances to Menu.
//
// Multi-cat (N>=2): rotate cycles the active cat; kicker prefixes
// the active cat's name. Cat image stays mood-mapped — the design
// keeps Idle's cat reflecting mood, not the per-cat slug (slug
// shows in Feed Confirm / Cat Edit instead).
// Adaptive UI rule: with N=1 the rotate handler does nothing and
// the kicker shows just "fed Xm ago" (the cat is implicit).
//
// Layout (240x240 round screen):
//    32 px from top  : time          "H:MM"   Montserrat 24 (Georgia 38 in design)
//    70 px from top  : kicker        "fed Xm ago"  or  "<cat>  ·  fed Xm ago"
//   centre + 18 y    : cat hero      130 px PNG, mood-mapped
//   bottom + 22 y    : footer        "next HH:MM <label>"  (from active cat's schedule)
class IdleView : public IView {
public:
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }

    const char* name() const override { return "idle"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    feedme::domain::CatRoster* roster_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* timeLbl_   = nullptr;
    lv_obj_t* kickerLbl_ = nullptr;
    lv_obj_t* catImg_    = nullptr;
    lv_obj_t* footerLbl_ = nullptr;

    feedme::ports::DisplayFrame lastFrame_{};
    int  lastDrawnActiveIdx_  = -1;
    int  lastDrawnRosterCount_ = -1;
    bool firstRender_ = true;
};

}  // namespace feedme::views
