#include "views/QuietHoursEditView.h"

#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void QuietHoursEditView::build(lv_obj_t* parent) {
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
    lv_label_set_text(fieldLbl_, "QUIET  START");
    lv_obj_align(fieldLbl_, LV_ALIGN_TOP_MID, 0, 56);

    // HH : MM in the centre. Editing only one of {start, end} at a time
    // — focus is shown via accent tint on the active sub-field, with
    // the field name above ("QUIET START" / "QUIET END") naming the
    // edge being edited.
    hourLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_font(hourLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(hourLbl_, "22");
    lv_obj_align(hourLbl_, LV_ALIGN_CENTER, -38, -8);

    colonLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(colonLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(colonLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(colonLbl_, ":");
    lv_obj_align(colonLbl_, LV_ALIGN_CENTER, 0, -12);

    minuteLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_font(minuteLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(minuteLbl_, "00");
    lv_obj_align(minuteLbl_, LV_ALIGN_CENTER, 38, -8);

    // Small "→ end" hint when editing start (and "← start" when editing
    // end) so the user knows what's coming next.
    arrowLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(arrowLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(arrowLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(arrowLbl_, "");
    lv_obj_align(arrowLbl_, LV_ALIGN_CENTER, 0, 36);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "TURN ADJ  PRESS NEXT");
    lv_obj_set_width(hint_, 160);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void QuietHoursEditView::applyFocusStyles() {
    const bool editingStart = (focus_ == Field::StartHour
                               || focus_ == Field::StartMinute);
    const bool focusHour    = (focus_ == Field::StartHour
                               || focus_ == Field::EndHour);

    lv_label_set_text(fieldLbl_, editingStart ? "QUIET  START" : "QUIET  END");
    lv_label_set_text(arrowLbl_, editingStart ? "next: end" : "save on press");

    lv_obj_set_style_text_color(hourLbl_,
        lv_color_hex(focusHour ? kTheme.accent : kTheme.ink), 0);
    lv_obj_set_style_text_color(minuteLbl_,
        lv_color_hex(focusHour ? kTheme.ink : kTheme.accent), 0);
}

void QuietHoursEditView::redraw() {
    if (!quiet_) return;
    const bool editingStart = (focus_ == Field::StartHour
                               || focus_ == Field::StartMinute);
    const int h = editingStart ? quiet_->startHour()   : quiet_->endHour();
    const int m = editingStart ? quiet_->startMinute() : quiet_->endMinute();

    const bool changed = firstRender_
                         || quiet_->startHour()   != lastDrawnSH_
                         || quiet_->startMinute() != lastDrawnSM_
                         || quiet_->endHour()     != lastDrawnEH_
                         || quiet_->endMinute()   != lastDrawnEM_
                         || focus_ != lastDrawnFocus_;
    if (!changed) return;

    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", h);
    lv_label_set_text(hourLbl_, buf);
    snprintf(buf, sizeof(buf), "%02d", m);
    lv_label_set_text(minuteLbl_, buf);

    applyFocusStyles();

    lastDrawnSH_     = quiet_->startHour();
    lastDrawnSM_     = quiet_->startMinute();
    lastDrawnEH_     = quiet_->endHour();
    lastDrawnEM_     = quiet_->endMinute();
    lastDrawnFocus_  = focus_;
    firstRender_     = false;
}

void QuietHoursEditView::onEnter() {
    focus_       = Field::StartHour;
    firstRender_ = true;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void QuietHoursEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void QuietHoursEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* QuietHoursEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!quiet_) return nullptr;
    switch (ev) {
        case E::RotateCW:
            switch (focus_) {
                case Field::StartHour:   quiet_->bumpStartHour(+1);   break;
                case Field::StartMinute: quiet_->bumpStartMinute(+1); break;
                case Field::EndHour:     quiet_->bumpEndHour(+1);     break;
                case Field::EndMinute:   quiet_->bumpEndMinute(+1);   break;
            }
            return nullptr;
        case E::RotateCCW:
            switch (focus_) {
                case Field::StartHour:   quiet_->bumpStartHour(-1);   break;
                case Field::StartMinute: quiet_->bumpStartMinute(-1); break;
                case Field::EndHour:     quiet_->bumpEndHour(-1);     break;
                case Field::EndMinute:   quiet_->bumpEndMinute(-1);   break;
            }
            return nullptr;
        case E::Tap:
        case E::Press:
            switch (focus_) {
                case Field::StartHour:   focus_ = Field::StartMinute; return nullptr;
                case Field::StartMinute: focus_ = Field::EndHour;     return nullptr;
                case Field::EndHour:     focus_ = Field::EndMinute;   return nullptr;
                case Field::EndMinute:   return "settings";  // save = mutations already applied
            }
            return nullptr;
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
