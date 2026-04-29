#include "views/PortionAdjustView.h"

#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void PortionAdjustView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Big portion number — Georgia 64 in design; Montserrat 48 here is
    // the largest size LVGL ships built-in. Add a -DFEEDME_FONT_LARGE
    // build flag in a follow-up if we want closer fidelity.
    numLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(numLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(numLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(numLbl_, "40");
    lv_obj_align(numLbl_, LV_ALIGN_CENTER, 0, -16);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(unitLbl_, "GRAMS");
    lv_obj_align(unitLbl_, LV_ALIGN_CENTER, 0, 30);

    // Side −/+ markers showing knob direction.
    minusLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(minusLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(minusLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(minusLbl_, "-");
    lv_obj_align(minusLbl_, LV_ALIGN_LEFT_MID, 30, -8);

    plusLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(plusLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(plusLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(plusLbl_, "+");
    lv_obj_align(plusLbl_, LV_ALIGN_RIGHT_MID, -30, -8);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "TAP  SAVE");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void PortionAdjustView::redraw() {
    if (!roster_ || roster_->count() == 0) return;
    const int g = roster_->activePortion().grams();
    if (g == lastDrawnG_) return;
    lastDrawnG_ = g;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g);
    lv_label_set_text(numLbl_, buf);
}

void PortionAdjustView::onEnter() {
    lastDrawnG_ = -1;
    redraw();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PortionAdjustView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PortionAdjustView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* PortionAdjustView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!roster_ || roster_->count() == 0) return nullptr;
    switch (ev) {
        case E::RotateCW:  roster_->activePortion().bumpUp();   return nullptr;
        case E::RotateCCW: roster_->activePortion().bumpDown(); return nullptr;
        case E::Tap:
        case E::Press:     return "feedConfirm";
        // Long-press / long-touch → ScreenManager fallback to parent()
        // (= "feedConfirm" — the prior buggy "menu" return skipped a
        // level and is removed).
        default:           return nullptr;
    }
}

}  // namespace feedme::views
