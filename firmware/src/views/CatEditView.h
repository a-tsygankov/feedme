#pragma once

#include "domain/CatRoster.h"
#include "views/IView.h"

namespace feedme::views {

// Phase D.5 — Single-cat editor.
//
// v0 scope: pose (slug) picker + avatar color picker. The cat's name
// is auto-assigned at add time ("Cat <id>") and is not editable
// on-device — name editing requires either the captive portal
// (roadmap Phase 2.4) or a knob character picker (Phase 1.3 style).
//
// Two focusable fields cycled by Tap/Press:
//   Field::Pose  — rotate cycles through kAvailableSlugs (5 entries).
//                  The hero image and slug label update live.
//   Field::Color — rotate cycles through Palette::kCatPalette. The
//                  hero image's recolor + slug label color reflect
//                  the new tint live.
//
// Gestures:
//   RotateCW / RotateCCW → cycle the focused field's value
//   Press / Tap          → advance focus (Pose → Color → save & exit)
//   Long-press           → ScreenManager fallback to parent (catsList);
//                          mutations already applied via roster setters,
//                          so "save" is just leaving.
class CatEditView : public IView {
public:
    void setRoster(feedme::domain::CatRoster* roster) { roster_ = roster; }
    void setEditingCatIndex(int catIdx) { catIdx_ = catIdx; }

    const char* name()   const override { return "catEdit"; }
    const char* parent() const override { return "catsList"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    enum class Field { Pose, Color };

    void redraw();
    void cycleSlug(int delta);
    void cycleColor(int delta);

    feedme::domain::CatRoster* roster_ = nullptr;
    int catIdx_ = -1;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* nameLbl_   = nullptr;
    lv_obj_t* catImg_    = nullptr;
    lv_obj_t* slugLbl_   = nullptr;
    lv_obj_t* fieldLbl_  = nullptr;   // "POSE" / "COLOR" — what rotate adjusts
    lv_obj_t* hint_      = nullptr;

    Field   focus_            = Field::Pose;
    char    lastDrawnSlug_[4] = {0};
    uint32_t lastDrawnColor_  = 0;
    Field   lastDrawnFocus_   = Field::Pose;
    int     lastDrawnCatIdx_  = -1;
    bool    firstRender_      = true;
};

}  // namespace feedme::views
