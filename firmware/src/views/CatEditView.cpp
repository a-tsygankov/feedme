#include "views/CatEditView.h"

#include "assets/cats/CatSlug.h"
#include "domain/Palette.h"
#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

namespace feedme::views {

namespace {

// Find the index of `color` in the cat palette, or -1 if it's not a
// palette member. Used so cycleColor advances cleanly even if NVS
// holds a value not currently in the (possibly-revised) palette.
int findColorIdx(uint32_t color) {
    for (int i = 0; i < feedme::domain::kCatPaletteSize; ++i) {
        if (feedme::domain::kCatPalette[i].rgb == color) return i;
    }
    return -1;
}

}  // namespace

void CatEditView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    nameLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(nameLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(nameLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(nameLbl_, "");
    lv_obj_align(nameLbl_, LV_ALIGN_TOP_MID, 0, 36);

    // Cat image preview — defaults to C2 happy at 88 px until redraw
    // populates it from the editing cat's slug.
    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, feedme::assets::slugToPath("C2", 88));
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, -8);

    slugLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(slugLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(slugLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(slugLbl_, "C2");
    lv_obj_align(slugLbl_, LV_ALIGN_BOTTOM_MID, 0, -56);

    // Small "what does rotate adjust right now?" indicator. Sits
    // between the slug label and the bottom hint.
    fieldLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(fieldLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(fieldLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(fieldLbl_, "POSE");
    lv_obj_align(fieldLbl_, LV_ALIGN_BOTTOM_MID, 0, -38);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN PICK  TAP NEXT");
    applyClippedLabel(hint_, 160);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void CatEditView::cycleSlug(int delta) {
    if (!roster_ || catIdx_ < 0 || catIdx_ >= roster_->count()) return;
    const auto& c = roster_->at(catIdx_);
    const int  cur = feedme::assets::slugIndex(c.slug);
    const int  N   = feedme::assets::kAvailableSlugCount;
    const int  next = ((cur + delta) % N + N) % N;
    roster_->setSlug(catIdx_, feedme::assets::kAvailableSlugs[next]);
}

void CatEditView::cycleColor(int delta) {
    if (!roster_ || catIdx_ < 0 || catIdx_ >= roster_->count()) return;
    const auto& c = roster_->at(catIdx_);
    const int N   = feedme::domain::kCatPaletteSize;
    int cur = findColorIdx(c.avatarColor);
    if (cur < 0) cur = 0;  // current value isn't in the palette → start at 0
    const int next = ((cur + delta) % N + N) % N;
    roster_->setAvatarColor(catIdx_, feedme::domain::kCatPalette[next].rgb);
}

void CatEditView::redraw() {
    if (!roster_ || catIdx_ < 0 || catIdx_ >= roster_->count()) return;
    const auto& c = roster_->at(catIdx_);
    const bool changed = firstRender_
                         || catIdx_         != lastDrawnCatIdx_
                         || strncmp(c.slug, lastDrawnSlug_, 4) != 0
                         || c.avatarColor   != lastDrawnColor_
                         || focus_          != lastDrawnFocus_;
    if (!changed) return;

    lv_label_set_text(nameLbl_, c.name);
    lv_label_set_text(slugLbl_, c.slug);
    lv_img_set_src(catImg_, feedme::assets::slugToPath(c.slug, 88));

    // Tint the preview by the current avatar color so the user sees
    // the effect of the color cycle live.
    lv_obj_set_style_img_recolor(catImg_, lv_color_hex(c.avatarColor), 0);
    lv_obj_set_style_img_recolor_opa(catImg_, LV_OPA_COVER, 0);
    // Slug label echoes the avatar tint when Color is focused (the
    // value the user is currently rotating); falls back to accent
    // when Pose is focused so it doesn't compete with the preview.
    lv_obj_set_style_text_color(slugLbl_,
        lv_color_hex(focus_ == Field::Color ? c.avatarColor : kTheme.accent), 0);

    lv_label_set_text(fieldLbl_, focus_ == Field::Pose ? "POSE" : "COLOR");

    strncpy(lastDrawnSlug_, c.slug, 3);
    lastDrawnSlug_[3] = '\0';
    lastDrawnColor_  = c.avatarColor;
    lastDrawnFocus_  = focus_;
    lastDrawnCatIdx_ = catIdx_;
    firstRender_     = false;
}

void CatEditView::onEnter() {
    focus_       = Field::Pose;   // start on the pose field each entry
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void CatEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* CatEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::RotateCW:
            if (focus_ == Field::Pose) cycleSlug(+1);
            else                       cycleColor(+1);
            return nullptr;
        case E::RotateCCW:
            if (focus_ == Field::Pose) cycleSlug(-1);
            else                       cycleColor(-1);
            return nullptr;
        case E::Tap:
        case E::Press:
            // Advance focus: Pose → Color → save & return.
            if (focus_ == Field::Pose) {
                focus_ = Field::Color;
                return nullptr;
            }
            return "catsList";  // mutations already applied by setters
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
