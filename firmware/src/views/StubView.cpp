#include "views/StubView.h"

#include "views/Theme.h"

namespace feedme::views {

StubView::StubView(const char* viewName, const char* label)
    : name_(viewName), label_(label) {}

void StubView::build(lv_obj_t* parent) {
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
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(titleLbl_, label_);
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, -8);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "press to go back");
    lv_obj_align(hintLbl_, LV_ALIGN_CENTER, 0, 22);
}

void StubView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void StubView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void StubView::render(const feedme::ports::DisplayFrame&) {
    // No state to display — stubs are static.
}

const char* StubView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (ev == E::Tap || ev == E::Press) return "menu";
    if (ev == E::DoubleTap || ev == E::DoublePress) return "idle";
    return nullptr;
}

}  // namespace feedme::views
