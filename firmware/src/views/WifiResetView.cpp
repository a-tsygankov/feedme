#include "views/WifiResetView.h"

#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

void WifiResetView::build(lv_obj_t* parent) {
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
    lv_obj_set_style_text_font(iconLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(iconLbl_, LV_SYMBOL_WIFI);
    lv_obj_align(iconLbl_, LV_ALIGN_TOP_MID, 0, 56);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Switch Wi-Fi");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, -8);

    bodyLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(bodyLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(bodyLbl_, &lv_font_montserrat_14, 0);
    // Two short lines fit better on the round screen than one long one.
    lv_label_set_text(bodyLbl_, "reboot to setup");
    lv_obj_align(bodyLbl_, LV_ALIGN_CENTER, 0, 22);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    // Bottom of a 240 px round screen: visible chord ≈ 138 px at y=218.
    // Old "PRESS CONFIRM TURN CANCEL" (28 chars) clipped on both edges.
    lv_label_set_text(hint_, "PRESS  OK   TURN  X");
    lv_obj_set_width(hint_, 140);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void WifiResetView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WifiResetView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WifiResetView::render(const feedme::ports::DisplayFrame&) {
    // Static — no per-frame refresh.
}

const char* WifiResetView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::Tap:
        case E::Press:
            Serial.println("[wifi] reset confirmed — invoking callback");
            if (onConfirm_) onConfirm_();
            // If onConfirm reboots (real hardware), this return is dead.
            // On simulator there's no reboot; bounce back to settings.
            return "settings";
        case E::RotateCW:
        case E::RotateCCW:
            return "settings";  // cancel
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
