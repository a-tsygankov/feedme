#pragma once

#include "views/IView.h"

namespace feedme::views {

// 02 Idle — time + ambient cat + footer hint.
// Per FeedMeKnob handoff: tap or knob press advances to Menu.
//
// Layout (240x240 round screen):
//    32 px from top  : time          "H:MM"   Montserrat 24 (Georgia 38 in design)
//    70 px from top  : kicker        "fed Xm ago"
//   centre + 18 y    : cat hero      130 px PNG, mood-mapped
//   bottom + 22 y    : footer        "next 13:00 lunch" (placeholder)
class IdleView : public IView {
public:
    const char* name() const override { return "idle"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    lv_obj_t* root_      = nullptr;
    lv_obj_t* timeLbl_   = nullptr;
    lv_obj_t* kickerLbl_ = nullptr;
    lv_obj_t* catImg_    = nullptr;
    lv_obj_t* footerLbl_ = nullptr;

    feedme::ports::DisplayFrame lastFrame_{};
    bool firstRender_ = true;
};

}  // namespace feedme::views
