#include "views/TimeZoneEditView.h"

#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

namespace {

void formatOffset(int minutes, char* buf, int bufLen) {
    if (minutes == 0) { snprintf(buf, bufLen, "UTC"); return; }
    const char sign = minutes > 0 ? '+' : '-';
    const int absMin = minutes > 0 ? minutes : -minutes;
    const int h = absMin / 60;
    const int m = absMin % 60;
    if (m == 0) snprintf(buf, bufLen, "UTC %c%d",       sign, h);
    else        snprintf(buf, bufLen, "UTC %c%d:%02d",  sign, h, m);
}

}  // namespace

void TimeZoneEditView::build(lv_obj_t* parent) {
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
    lv_label_set_text(fieldLbl_, "TIMEZONE");
    lv_obj_align(fieldLbl_, LV_ALIGN_TOP_MID, 0, 56);

    valueLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(valueLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(valueLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(valueLbl_, "UTC");
    lv_obj_align(valueLbl_, LV_ALIGN_CENTER, 0, -8);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN HOURS  PRESS SAVE");
    lv_obj_set_width(hint_, 160);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void TimeZoneEditView::redraw() {
    if (!tz_) return;
    const int m = tz_->offsetMin();
    if (m == lastDrawnMin_) return;
    lastDrawnMin_ = m;
    char buf[16];
    formatOffset(m, buf, sizeof(buf));
    lv_label_set_text(valueLbl_, buf);
}

void TimeZoneEditView::onEnter() {
    lastDrawnMin_ = INT32_MIN;
    redraw();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void TimeZoneEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void TimeZoneEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* TimeZoneEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!tz_) return nullptr;
    switch (ev) {
        case E::RotateCW:  tz_->bumpHour(+1); return nullptr;
        case E::RotateCCW: tz_->bumpHour(-1); return nullptr;
        case E::Tap:
        case E::Press:     return "settings";  // already mutated; persists on dirty
        default:           return nullptr;
    }
}

}  // namespace feedme::views
