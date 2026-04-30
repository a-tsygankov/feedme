#include "views/SetupView.h"

#include "views/Theme.h"

namespace feedme::views {

void SetupView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Wi-Fi setup");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 36);

    line1Lbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(line1Lbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(line1Lbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(line1Lbl_, "connect to");
    lv_obj_align(line1Lbl_, LV_ALIGN_TOP_MID, 0, 80);

    apLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(apLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(apLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(apLbl_, "feedme-?");
    lv_obj_align(apLbl_, LV_ALIGN_TOP_MID, 0, 100);

    line3Lbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(line3Lbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(line3Lbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(line3Lbl_, "then open");
    lv_obj_align(line3Lbl_, LV_ALIGN_TOP_MID, 0, 138);

    urlLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(urlLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(urlLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(urlLbl_, "192.168.4.1");
    lv_obj_align(urlLbl_, LV_ALIGN_TOP_MID, 0, 158);
}

void SetupView::onEnter() {
    lv_label_set_text(apLbl_,  apName_);
    lv_label_set_text(urlLbl_, url_);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SetupView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SetupView::render(const feedme::ports::DisplayFrame&) {
    // Static labels — nothing to refresh per frame.
}

const char* SetupView::handleInput(feedme::ports::TapEvent) {
    // No input in setup mode — interaction is via the captive portal
    // web form, not the knob/touch.
    return nullptr;
}

}  // namespace feedme::views
