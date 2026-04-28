#pragma once

#include "views/IView.h"

namespace feedme::views {

// 03 Menu — four orbiting glyphs (Feed / Schedule / Quiet / Settings).
// Per FeedMeKnob handoff: rotate to highlight, press/tap to open.
// Long-press anywhere → LockConfirm (handled by dispatcher).
//
// Geometry: 4 circles at -90 / 0 / 90 / 180 degrees on a R=70 px ring,
// centred. Selected item shown with an accent fill and ink-on-accent
// glyph. Centre carries the selected item's name + "press to open".
class MenuView : public IView {
public:
    static constexpr int ITEM_COUNT = 4;

    const char* name() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void applySelection();

    lv_obj_t* root_                  = nullptr;
    lv_obj_t* centreLabel_           = nullptr;
    lv_obj_t* centreHint_            = nullptr;
    lv_obj_t* glyphs_[ITEM_COUNT]    = {nullptr};
    lv_obj_t* glyphLabels_[ITEM_COUNT] = {nullptr};
    int       selected_              = 0;
};

}  // namespace feedme::views
