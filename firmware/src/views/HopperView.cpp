#include "views/HopperView.h"

#include "views/Theme.h"

namespace feedme::views {

namespace {

constexpr int ARC_RADIUS_PX  = 100;
constexpr int ARC_DIAM_PX    = ARC_RADIUS_PX * 2;
// Same 270° span centred on bottom as ScrFeedConfirm — start at the
// design's -135° (lower-left) and sweep CW.
constexpr int ARC_ROTATION   = 135;
constexpr int ARC_SWEEP_MAX  = 270;
// Static placeholder content — there is no load cell on this board.
constexpr int PLACEHOLDER_PCT = 32;
constexpr const char* PLACEHOLDER_DAYS = "~ 3 days remaining";

}  // namespace

void HopperView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Background ring.
    dialArc_ = lv_arc_create(root_);
    lv_obj_set_size(dialArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(dialArc_);
    lv_arc_set_rotation(dialArc_, ARC_ROTATION);
    lv_arc_set_bg_angles(dialArc_, 0, ARC_SWEEP_MAX);
    lv_obj_remove_style(dialArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(dialArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(dialArc_, lv_color_hex(kTheme.line), LV_PART_MAIN);
    lv_obj_set_style_arc_width(dialArc_, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(dialArc_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Fill arc.
    fillArc_ = lv_arc_create(root_);
    lv_obj_set_size(fillArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(fillArc_);
    lv_arc_set_rotation(fillArc_, ARC_ROTATION);
    lv_arc_set_bg_angles(fillArc_, 0, ARC_SWEEP_MAX);
    lv_arc_set_range(fillArc_, 0, 100);
    lv_arc_set_value(fillArc_, PLACEHOLDER_PCT);
    lv_obj_remove_style(fillArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(fillArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(fillArc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(fillArc_, lv_color_hex(kTheme.accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(fillArc_, 6, LV_PART_INDICATOR);

    // Big "32%" — Montserrat 48 (LVGL's largest built-in; design wants
    // Georgia 44 with smaller "%" suffix).
    pctLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(pctLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(pctLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(pctLbl_, "32");
    lv_obj_align(pctLbl_, LV_ALIGN_TOP_MID, -8, 64);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(unitLbl_, "%");
    lv_obj_align(unitLbl_, LV_ALIGN_TOP_MID, 26, 78);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(titleLbl_, "HOPPER");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 134);

    daysLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(daysLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(daysLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(daysLbl_, PLACEHOLDER_DAYS);
    lv_obj_align(daysLbl_, LV_ALIGN_TOP_MID, 0, 158);
}

void HopperView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void HopperView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void HopperView::render(const feedme::ports::DisplayFrame&) {
    // Static content; no per-frame refresh until a real load-cell
    // adapter exposes a live percentage.
}

const char* HopperView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (ev == E::Tap || ev == E::Press) return "idle";
    return nullptr;
}

}  // namespace feedme::views
