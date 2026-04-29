#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// 10 Portion Adjust — big number editor. Per FeedMeKnob handoff §3:
//   knob:  rotate ±5 g
//   touch: tap to save (return to FeedConfirm)
//
// The portion mutates the same shared PortionState as FeedConfirmView,
// so "save" is just leaving — main.cpp persists on consumeDirty().
class PortionAdjustView : public IView {
public:
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }

    const char* name() const override { return "portionAdjust"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::domain::CatRoster* roster_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* numLbl_    = nullptr;
    lv_obj_t* unitLbl_   = nullptr;
    lv_obj_t* minusLbl_  = nullptr;
    lv_obj_t* plusLbl_   = nullptr;
    lv_obj_t* hintLbl_   = nullptr;
    int       lastDrawnG_ = -1;
};

}  // namespace feedme::views
