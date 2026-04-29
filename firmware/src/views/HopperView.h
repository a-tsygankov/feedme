#pragma once

#include "views/IView.h"

namespace feedme::views {

// 09 Hopper level — big % + 270° fill arc + days-remaining hint.
// Per FeedMeKnob handoff §3: read-only, tap → idle.
//
// Phase C.6: feature-flagged via -DFEEDME_HAS_HOPPER. The view always
// compiles so the class is available, but LvglDisplay only registers
// it (and any future menu entry) when the flag is set. The current
// device has no load cell — placeholder content (32%, ~3 days) is
// static until a real hopper sensor adapter lands.
class HopperView : public IView {
public:
    const char* name()   const override { return "hopper"; }
    const char* parent() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    lv_obj_t* root_     = nullptr;
    lv_obj_t* dialArc_  = nullptr;
    lv_obj_t* fillArc_  = nullptr;
    lv_obj_t* pctLbl_   = nullptr;
    lv_obj_t* unitLbl_  = nullptr;
    lv_obj_t* titleLbl_ = nullptr;
    lv_obj_t* daysLbl_  = nullptr;
};

}  // namespace feedme::views
