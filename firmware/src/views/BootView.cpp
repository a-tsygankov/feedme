#include "views/BootView.h"

#include "assets/cats/CatSlug.h"
#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

namespace {

constexpr int CAT_SIZE_PX = 88;
constexpr int DOT_SIZE_PX = 5;
constexpr int DOT_GAP_PX  = 6;

}  // namespace

void BootView::build(lv_obj_t* parent) {
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
    lv_img_set_src(catImg_, feedme::assets::slugToPath("B1", 88));
    lv_obj_set_style_img_opa(catImg_, LV_OPA_90, 0);
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, -34);

    nameLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(nameLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(nameLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(nameLbl_, "FeedMe");
    lv_obj_align(nameLbl_, LV_ALIGN_CENTER, 0, 24);

    // Three small accent dots; middle one brighter (per JSX).
    const int totalWidth = 3 * DOT_SIZE_PX + 2 * DOT_GAP_PX;
    const int startX = -totalWidth / 2 + DOT_SIZE_PX / 2;
    for (int i = 0; i < 3; ++i) {
        dot_[i] = lv_obj_create(root_);
        lv_obj_set_size(dot_[i], DOT_SIZE_PX, DOT_SIZE_PX);
        lv_obj_set_style_radius(dot_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot_[i], lv_color_hex(kTheme.accent), 0);
        lv_obj_set_style_bg_opa(dot_[i], (i == 1) ? LV_OPA_COVER : LV_OPA_30, 0);
        lv_obj_set_style_border_width(dot_[i], 0, 0);
        lv_obj_set_style_pad_all(dot_[i], 0, 0);
        lv_obj_clear_flag(dot_[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(dot_[i], LV_ALIGN_CENTER,
                     startX + i * (DOT_SIZE_PX + DOT_GAP_PX), 56);
    }
}

void BootView::onEnter() {
    enteredMs_ = millis();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void BootView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void BootView::render(const feedme::ports::DisplayFrame&) {
    // No state to refresh; nextView() drives the timed dismissal.
}

const char* BootView::nextView() {
    if (millis() - enteredMs_ < BOOT_DURATION_MS) return nullptr;
    if (nextOverride_ && nextOverride_[0]) return nextOverride_;
    return "idle";
}

}  // namespace feedme::views
