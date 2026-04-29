#include "views/PouringView.h"

#include "application/FeedingService.h"
#include "assets/cats/CatSlug.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {

constexpr int ARC_RADIUS = 105;
constexpr int ARC_DIAM   = ARC_RADIUS * 2;
// Full perimeter ring starting at 12 o'clock.
constexpr int ARC_ROTATION = 270;
constexpr int ARC_SWEEP_MAX = 360;

}  // namespace

void PouringView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Background full ring.
    arcBg_ = lv_arc_create(root_);
    lv_obj_set_size(arcBg_, ARC_DIAM, ARC_DIAM);
    lv_obj_center(arcBg_);
    lv_arc_set_rotation(arcBg_, ARC_ROTATION);
    lv_arc_set_bg_angles(arcBg_, 0, ARC_SWEEP_MAX);
    lv_arc_set_value(arcBg_, 0);
    lv_obj_remove_style(arcBg_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arcBg_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arcBg_, lv_color_hex(kTheme.line), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arcBg_, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arcBg_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Foreground filling arc.
    arcFg_ = lv_arc_create(root_);
    lv_obj_set_size(arcFg_, ARC_DIAM, ARC_DIAM);
    lv_obj_center(arcFg_);
    lv_arc_set_rotation(arcFg_, ARC_ROTATION);
    lv_arc_set_bg_angles(arcFg_, 0, ARC_SWEEP_MAX);
    lv_arc_set_range(arcFg_, 0, ARC_SWEEP_MAX);
    lv_arc_set_value(arcFg_, 0);
    lv_obj_remove_style(arcFg_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arcFg_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(arcFg_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arcFg_, lv_color_hex(kTheme.accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arcFg_, 3, LV_PART_INDICATOR);

    // Faded neutral cat at top.
    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, feedme::assets::slugToPath("B1", 88));
    lv_obj_set_style_img_opa(catImg_, LV_OPA_50, 0);
    lv_obj_align(catImg_, LV_ALIGN_TOP_MID, 0, 50);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(titleLbl_, "Pouring");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, 6);

    progressLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(progressLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(progressLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(progressLbl_, "0 g of 40");
    lv_obj_align(progressLbl_, LV_ALIGN_CENTER, 0, 36);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "HOLD  CANCEL");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void PouringView::onEnter() {
    startMs_   = millis();
    completed_ = false;
    cancelled_ = false;
    lastSweep_ = -1;
    lv_arc_set_value(arcFg_, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PouringView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PouringView::render(const feedme::ports::DisplayFrame&) {
    if (cancelled_ || !roster_ || roster_->count() == 0) return;

    const uint32_t elapsed = millis() - startMs_;
    const float progress = (elapsed >= POUR_DURATION_MS)
                               ? 1.0f
                               : static_cast<float>(elapsed) / POUR_DURATION_MS;

    const int sweep = static_cast<int>(progress * ARC_SWEEP_MAX);
    if (sweep != lastSweep_) {
        lastSweep_ = sweep;
        lv_arc_set_value(arcFg_, sweep);

        const int total = roster_->activePortion().grams();
        const int poured = static_cast<int>(progress * total);
        char buf[24];
        snprintf(buf, sizeof(buf), "%d g of %d", poured, total);
        lv_label_set_text(progressLbl_, buf);
    }

    if (!completed_ && elapsed >= POUR_DURATION_MS) {
        completed_ = true;
        // currentFeederName() returns the picker selection if one was
        // made (N≥2 path), or primaryName() as the silent N=1 fallback.
        const char* owner = users_ ? users_->currentFeederName() : "you";
        // FeedConfirm sets roster.feedSelection() before transitioning
        // here: FEED_ALL = log every cat in the roster (default for
        // multi-cat homes), or a specific slot index = log just that
        // cat. Each cat's individual portion is honored via
        // FeedingService → roster.at(slot).portion.
        if (feeding_ && roster_) {
            const int sel = roster_->feedSelection();
            if (sel == feedme::domain::CatRoster::FEED_ALL) {
                for (int i = 0; i < roster_->count(); ++i) {
                    feeding_->logFeeding(owner, i);
                }
            } else if (sel >= 0 && sel < roster_->count()) {
                feeding_->logFeeding(owner, sel);
            }
        }
        // Reset the transient picker selection so the next feed starts
        // fresh — devices are shared, no remembered "current user".
        if (users_) users_->clearCurrentFeeder();
    }
}

const char* PouringView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (ev == E::LongPress || ev == E::LongTouch) {
        cancelled_ = true;
        return "menu";
    }
    return nullptr;
}

const char* PouringView::nextView() {
    if (completed_) return "fed";
    return nullptr;
}

}  // namespace feedme::views
