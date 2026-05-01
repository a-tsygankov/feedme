#pragma once

#include "ports/ITapSensor.h"
#include "views/IView.h"

namespace feedme::views {

// 03 Menu — four orbiting glyphs (Feed / Schedule / Quiet / Settings).
// Per FeedMeKnob handoff: rotate to highlight, press/tap to open.
// Long-press anywhere → LockConfirm (handled by dispatcher).
//
// Geometry: 4 circles at -90 / 0 / 90 / 180 degrees on a R=70 px ring,
// centred. Selected item shown with an accent fill and ink-on-accent
// glyph. Centre carries the selected item's name + "press to open".
//
// Positional tap: when a touch sensor is wired in via setTouchSensor()
// AND the touch's x/y land inside one of the four glyph circles, the
// tap selects that glyph directly — no need to rotate to it first.
// Falls back to the original rotate-then-confirm behaviour when the
// sensor doesn't report coords or the touch lands outside any glyph.
class MenuView : public IView {
public:
    static constexpr int ITEM_COUNT = 4;

    void setTouchSensor(const feedme::ports::ITapSensor* s) { touch_ = s; }

    const char* name() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void applySelection();
    // Returns the glyph index whose circle contains (x,y), or -1 if
    // the point is outside all four. Uses the same orbit geometry as
    // the build() ring; coordinates are screen-pixel space (0..239).
    int  hitTest(int x, int y) const;

    const feedme::ports::ITapSensor* touch_ = nullptr;

    lv_obj_t* root_                  = nullptr;
    lv_obj_t* centreLabel_           = nullptr;
    lv_obj_t* centreHint_            = nullptr;
    lv_obj_t* glyphs_[ITEM_COUNT]    = {nullptr};
    lv_obj_t* glyphLabels_[ITEM_COUNT] = {nullptr};
    int       selected_              = 0;
};

}  // namespace feedme::views
