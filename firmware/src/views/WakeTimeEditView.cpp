#include "views/WakeTimeEditView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void WakeTimeEditView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Field name (small, top).
    fieldLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(fieldLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(fieldLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(fieldLbl_, "WAKE");
    lv_obj_align(fieldLbl_, LV_ALIGN_TOP_MID, 0, 56);

    // Big HH : MM in the centre. The colon is its own label so we can
    // re-tint hour vs minute independently to show focus.
    hourLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_font(hourLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(hourLbl_, "06");
    lv_obj_align(hourLbl_, LV_ALIGN_CENTER, -38, -8);

    colonLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(colonLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(colonLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(colonLbl_, ":");
    lv_obj_align(colonLbl_, LV_ALIGN_CENTER, 0, -12);

    minuteLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_font(minuteLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(minuteLbl_, "30");
    lv_obj_align(minuteLbl_, LV_ALIGN_CENTER, 38, -8);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "TURN ADJ  PRESS NEXT");
    applyClippedLabel(hintLbl_, 160);
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -24);

    addBackHint(root_);
}

void WakeTimeEditView::applyFocusStyles() {
    const bool focusHour = (focus_ == Field::Hour);
    lv_obj_set_style_text_color(hourLbl_,
        lv_color_hex(focusHour ? kTheme.accent : kTheme.ink), 0);
    lv_obj_set_style_text_color(minuteLbl_,
        lv_color_hex(focusHour ? kTheme.ink : kTheme.accent), 0);
}

void WakeTimeEditView::redraw() {
    if (!wake_) return;
    const int h = wake_->hour();
    const int m = wake_->minute();
    const bool changed = firstRender_
                         || h      != lastDrawnHour_
                         || m      != lastDrawnMinute_
                         || focus_ != lastDrawnFocus_;
    if (!changed) return;

    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", h);
    lv_label_set_text(hourLbl_, buf);
    snprintf(buf, sizeof(buf), "%02d", m);
    lv_label_set_text(minuteLbl_, buf);

    applyFocusStyles();

    lastDrawnHour_   = h;
    lastDrawnMinute_ = m;
    lastDrawnFocus_  = focus_;
    firstRender_     = false;
}

void WakeTimeEditView::onEnter() {
    focus_       = Field::Hour;   // start on the hour
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WakeTimeEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WakeTimeEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* WakeTimeEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!wake_) return nullptr;
    switch (ev) {
        case E::RotateCW:
            if (focus_ == Field::Hour) wake_->bumpHour(+1);
            else                       wake_->bumpMinute(+1);
            return nullptr;
        case E::RotateCCW:
            if (focus_ == Field::Hour) wake_->bumpHour(-1);
            else                       wake_->bumpMinute(-1);
            return nullptr;
        case E::Tap:
        case E::Press:
            if (focus_ == Field::Hour) {
                focus_ = Field::Minute;
                return nullptr;
            }
            // Final press → save (already mutating WakeTime in place,
            // main.cpp will persist on consumeDirty()) → back to Settings.
            return "settings";
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
