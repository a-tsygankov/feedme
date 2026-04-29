#include "views/FeedConfirmView.h"

#include "assets/cats/CatSlug.h"
#include "views/Theme.h"

#include <stdio.h>
#include <string.h>

namespace feedme::views {

namespace {

// Portion arc spans 270° centred at the bottom (per ScrFeedConfirm:
// arcPath -135 → +135). LVGL arc angles measure CW from 3 o'clock,
// so -135° design = 135° LVGL, +135° design = 45° LVGL going around
// the long way. lv_arc rotates via set_rotation so we can use the
// natural 0..270 sweep range.
constexpr int ARC_RADIUS = 105;
constexpr int ARC_DIAM   = ARC_RADIUS * 2;
// Rotation: we want the arc to start at the design's -135° (lower-left)
// and sweep CW through the top to +135° (lower-right). That's the same
// as starting at 135° in LVGL coords with an end angle 135 + 270 = 405
// (= 45° wrapped). Using rotation simplifies: rotate by 135, sweep 0..270.
constexpr int ARC_ROTATION = 135;
constexpr int ARC_SWEEP_MAX_DEG = 270;

}  // namespace

void FeedConfirmView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Background arc (the unfilled portion track).
    arcBg_ = lv_arc_create(root_);
    lv_obj_set_size(arcBg_, ARC_DIAM, ARC_DIAM);
    lv_obj_center(arcBg_);
    lv_arc_set_rotation(arcBg_, ARC_ROTATION);
    lv_arc_set_bg_angles(arcBg_, 0, ARC_SWEEP_MAX_DEG);
    lv_arc_set_value(arcBg_, 0);
    lv_obj_remove_style(arcBg_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arcBg_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arcBg_, lv_color_hex(kTheme.line), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arcBg_, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arcBg_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Foreground arc (filled portion).
    arcFg_ = lv_arc_create(root_);
    lv_obj_set_size(arcFg_, ARC_DIAM, ARC_DIAM);
    lv_obj_center(arcFg_);
    lv_arc_set_rotation(arcFg_, ARC_ROTATION);
    lv_arc_set_bg_angles(arcFg_, 0, ARC_SWEEP_MAX_DEG);
    lv_arc_set_range(arcFg_, 0, ARC_SWEEP_MAX_DEG);
    lv_arc_set_value(arcFg_, 0);
    lv_obj_remove_style(arcFg_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arcFg_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(arcFg_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arcFg_, lv_color_hex(kTheme.accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arcFg_, 4, LV_PART_INDICATOR);

    // Hungry cat at top (88 px). Per JSX: top:58, w/h:88. Source is
    // overwritten in redraw() with the active cat's slug — this is
    // just a sensible build-time default so the widget has dimensions.
    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, feedme::assets::slugToPath("B2", 88));
    lv_obj_align(catImg_, LV_ALIGN_TOP_MID, 0, 58);

    // Portion number, Georgia in design — Montserrat 24 here. Top:152.
    portionLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(portionLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(portionLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(portionLbl_, "40");
    lv_obj_align(portionLbl_, LV_ALIGN_TOP_MID, -8, 148);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(unitLbl_, "g");
    lv_obj_align(unitLbl_, LV_ALIGN_TOP_MID, 22, 156);

    // Kicker hint, faint uppercase.
    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "TURN  ADJ  PRESS  POUR");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void FeedConfirmView::redraw() {
    if (!roster_ || roster_->count() == 0) return;
    const auto& cat = roster_->active();
    const int g = cat.portion.grams();
    const int activeIdx = roster_->activeCatIdx();

    // Hero image — active cat's slug → LittleFS path. Bad slugs land
    // on C2 (happy) inside slugToPath().
    if (activeIdx != lastDrawnActiveIdx_
        || strncmp(cat.slug, lastDrawnSlug_, 4) != 0) {
        lv_img_set_src(catImg_, feedme::assets::slugToPath(cat.slug, 88));
        strncpy(lastDrawnSlug_, cat.slug, 3);
        lastDrawnSlug_[3]   = '\0';
        lastDrawnActiveIdx_ = activeIdx;
    }

    if (g == lastDrawnG_) return;
    lastDrawnG_ = g;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", g);
    lv_label_set_text(portionLbl_, buf);

    // Arc sweep 0..270 mapped to portion 0..MAX_G (60). LVGL takes int.
    const int sweep = (g * ARC_SWEEP_MAX_DEG) / feedme::domain::PortionState::MAX_G;
    lv_arc_set_value(arcFg_, sweep);
}

void FeedConfirmView::onEnter() {
    lastDrawnG_         = -1;  // force a redraw
    lastDrawnActiveIdx_ = -1;
    lastDrawnSlug_[0]   = '\0';
    redraw();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void FeedConfirmView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void FeedConfirmView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* FeedConfirmView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!roster_ || roster_->count() == 0) return nullptr;
    switch (ev) {
        case E::RotateCW:  roster_->activePortion().bumpUp();   return nullptr;
        case E::RotateCCW: roster_->activePortion().bumpDown(); return nullptr;
        case E::Press:
            // Adaptive: N≥2 users → ask who's feeding before logging.
            if (users_ && users_->count() >= 2) return "feederPick";
            return "pouring";
        case E::Tap:       return "portionAdjust";
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:           return nullptr;
    }
}

}  // namespace feedme::views
