#include "views/ThresholdEditView.h"

#include "application/DisplayCoordinator.h"
#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void ThresholdEditView::build(lv_obj_t* parent) {
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
    lv_label_set_text(fieldLbl_, "HUNGRY  AFTER");
    lv_obj_align(fieldLbl_, LV_ALIGN_TOP_MID, 0, 56);

    valueLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(valueLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(valueLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(valueLbl_, "5h");
    lv_obj_align(valueLbl_, LV_ALIGN_CENTER, 0, -12);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(unitLbl_, "30 min steps  -  range 0:30 to 12:00");
    lv_obj_align(unitLbl_, LV_ALIGN_CENTER, 0, 36);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN ADJ  PRESS SAVE");
    lv_obj_set_width(hint_, 160);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void ThresholdEditView::redraw() {
    if (!coord_) return;
    const int64_t sec = coord_->hungryThresholdSec();
    if (sec == lastDrawnSec_) return;
    lastDrawnSec_ = sec;

    const int totalMin = static_cast<int>(sec / 60);
    const int hours    = totalMin / 60;
    const int minutes  = totalMin % 60;

    char buf[12];
    if (minutes == 0) snprintf(buf, sizeof(buf), "%dh",      hours);
    else              snprintf(buf, sizeof(buf), "%dh %02d", hours, minutes);
    lv_label_set_text(valueLbl_, buf);
}

void ThresholdEditView::onEnter() {
    lastDrawnSec_ = -1;
    redraw();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ThresholdEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ThresholdEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* ThresholdEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!coord_) return nullptr;
    switch (ev) {
        case E::RotateCW:  coord_->adjustHungryThreshold(+STEP_SEC); return nullptr;
        case E::RotateCCW: coord_->adjustHungryThreshold(-STEP_SEC); return nullptr;
        case E::Tap:
        case E::Press:     return "settings";  // already persisted on each adjust
        default:           return nullptr;
    }
}

}  // namespace feedme::views
