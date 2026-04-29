#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.5 — Single-cat editor.
//
// v0 scope: slug picker only. The cat's name is auto-assigned at add
// time ("Cat <id>") and is not editable on-device — name editing
// requires either the captive portal (roadmap Phase 2.4) or a knob
// character picker (Phase 1.3 style). The slug picker cycles through
// the 5 pre-converted cats (kAvailableSlugs).
//
// Gestures:
//   RotateCW / RotateCCW → cycle slug; preview image updates
//   Press / Tap          → save (already mutated through setSlug) and
//                          return to catsList
class CatEditView : public IView {
public:
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }
    void setEditingCatIndex(int catIdx) { catIdx_ = catIdx; }

    const char* name() const override { return "catEdit"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();
    void cycleSlug(int delta);

    feedme::domain::CatRoster* roster_ = nullptr;
    int catIdx_ = -1;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* nameLbl_   = nullptr;
    lv_obj_t* catImg_    = nullptr;
    lv_obj_t* slugLbl_   = nullptr;
    lv_obj_t* hint_      = nullptr;

    char    lastDrawnSlug_[4] = {0};
    int     lastDrawnCatIdx_  = -1;
    bool    firstRender_      = true;
};

}  // namespace feedme::views
