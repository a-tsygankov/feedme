#include "views/SleepTimeoutEditView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void SleepTimeoutEditView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    fieldLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(fieldLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(fieldLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(fieldLbl_, "SLEEP");
    lv_obj_align(fieldLbl_, LV_ALIGN_TOP_MID, 0, 50);

    valueLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(valueLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(valueLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(valueLbl_, "--");
    lv_obj_align(valueLbl_, LV_ALIGN_CENTER, 0, -8);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(unitLbl_, "OFF");
    lv_obj_align(unitLbl_, LV_ALIGN_CENTER, 0, 36);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN MIN  PRESS SAVE");
    applyClippedLabel(hint_, 160);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void SleepTimeoutEditView::redraw() {
    if (!sleep_) return;
    const int m = sleep_->minutes();
    if (m == lastDrawnMin_) return;
    lastDrawnMin_ = m;

    if (m == 0) {
        // Sleep disabled — "--" mirrors the Settings row's display.
        lv_label_set_text(valueLbl_, "--");
        lv_label_set_text(unitLbl_,  "OFF");
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", m);
        lv_label_set_text(valueLbl_, buf);
        lv_label_set_text(unitLbl_,  m == 1 ? "MINUTE" : "MINUTES");
    }
}

void SleepTimeoutEditView::onEnter() {
    lastDrawnMin_ = -1;
    redraw();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SleepTimeoutEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SleepTimeoutEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* SleepTimeoutEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!sleep_) return nullptr;
    switch (ev) {
        case E::RotateCW:  sleep_->bumpUp();   return nullptr;
        case E::RotateCCW: sleep_->bumpDown(); return nullptr;
        case E::Tap:
        case E::Press:     return "settings";   // already mutated; persists on dirty
        default:           return nullptr;
    }
}

}  // namespace feedme::views
