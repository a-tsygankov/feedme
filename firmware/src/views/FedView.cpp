#include "views/FedView.h"

#include "assets/cats/CatSlug.h"
#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

void FedView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, feedme::assets::slugToPath("C4", 130));
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, 0);

    // LVGL ships no built-in heart icon — use a stylised char as
    // placeholder. Replace with a proper line-icon (per
    // FeedMeKnobIcons.IcHeart) in a follow-up.
    heartLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(heartLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(heartLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(heartLbl_, LV_SYMBOL_OK);
    lv_obj_align(heartLbl_, LV_ALIGN_TOP_RIGHT, -60, 56);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(titleLbl_, "fed");
    lv_obj_align(titleLbl_, LV_ALIGN_BOTTOM_MID, 0, -38);

    footLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(footLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(footLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(footLbl_, "next  13:00");
    lv_obj_align(footLbl_, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void FedView::onEnter() {
    enteredMs_ = millis();
    dismissed_ = false;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void FedView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void FedView::render(const feedme::ports::DisplayFrame&) {
    if (!dismissed_ && millis() - enteredMs_ >= AUTO_DISMISS_MS) {
        dismissed_ = true;
    }
}

const char* FedView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (ev == E::Tap || ev == E::Press) {
        dismissed_ = true;
        return "idle";
    }
    return nullptr;
}

const char* FedView::nextView() {
    if (dismissed_) return "idle";
    return nullptr;
}

}  // namespace feedme::views
