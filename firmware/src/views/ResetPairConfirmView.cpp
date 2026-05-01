#include "views/ResetPairConfirmView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

void ResetPairConfirmView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    iconLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(iconLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(iconLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(iconLbl_, LV_SYMBOL_WARNING);
    lv_obj_align(iconLbl_, LV_ALIGN_TOP_MID, 0, 30);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Reset pairing?");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 56);

    // Body explains the consequence — new hid, old PIN unrecoverable.
    bodyLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(bodyLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(bodyLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(bodyLbl_, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(bodyLbl_, 200);
    lv_obj_set_style_text_align(bodyLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(bodyLbl_,
                      "New ID. Old PIN won't work. Device reboots.");
    lv_obj_align(bodyLbl_, LV_ALIGN_TOP_MID, 0, 96);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "tap to confirm");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -38);

    addBackHint(root_);   // ◀ hold = cancel
}

void ResetPairConfirmView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ResetPairConfirmView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ResetPairConfirmView::render(const feedme::ports::DisplayFrame&) {
    // Static — nothing to refresh.
}

const char* ResetPairConfirmView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::Tap:
        case E::Press:
            Serial.println("[pairing] reset confirmed — rotating hid + reboot");
            if (onConfirm_) onConfirm_();   // never returns (calls ESP.restart)
            // If somehow we get here (sim / no callback), bounce back.
            return "pairing";
        default:
            // Long-press / long-touch / rotation → cancel via parent().
            return nullptr;
    }
}

}  // namespace feedme::views
