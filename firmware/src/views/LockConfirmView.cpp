#include "views/LockConfirmView.h"

#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

namespace {

constexpr int ARC_RADIUS_PX = 100;
constexpr int ARC_DIAM_PX   = ARC_RADIUS_PX * 2;
constexpr int ARC_ROTATION  = 270;            // arc 0° = 12 o'clock
constexpr int ARC_SWEEP_MAX = 360;

}  // namespace

void LockConfirmView::build(lv_obj_t* parent) {
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
    lv_arc_set_bg_angles(dialArc_, 0, 359);
    lv_obj_remove_style(dialArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(dialArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(dialArc_, lv_color_hex(kTheme.line), LV_PART_MAIN);
    lv_obj_set_style_arc_width(dialArc_, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(dialArc_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Fill arc — accent, drives off elapsed-hold time.
    fillArc_ = lv_arc_create(root_);
    lv_obj_set_size(fillArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(fillArc_);
    lv_arc_set_rotation(fillArc_, ARC_ROTATION);
    lv_arc_set_bg_angles(fillArc_, 0, ARC_SWEEP_MAX);
    lv_arc_set_range(fillArc_, 0, ARC_SWEEP_MAX);
    lv_arc_set_value(fillArc_, 0);
    lv_obj_remove_style(fillArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(fillArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_opa(fillArc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(fillArc_, lv_color_hex(kTheme.accent), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(fillArc_, 5, LV_PART_INDICATOR);

    // Hold-icon stand-in. Design wants a custom IcHold pictogram; LVGL
    // ships none so use LV_SYMBOL_DOWN as a "press / hold" cue.
    iconLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(iconLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(iconLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(iconLbl_, LV_SYMBOL_DOWN);
    lv_obj_align(iconLbl_, LV_ALIGN_TOP_MID, 0, 70);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Hold to confirm");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, 0);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "cancel today's schedule");
    lv_obj_align(hintLbl_, LV_ALIGN_CENTER, 0, 28);
}

bool LockConfirmView::stillHeld() const {
    return (touch_  && touch_->isPressed())
        || (button_ && button_->isPressed());
}

void LockConfirmView::onEnter() {
    startMs_   = millis();
    completed_ = false;
    released_  = false;
    lastSweep_ = -1;
    lv_arc_set_value(fillArc_, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void LockConfirmView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void LockConfirmView::render(const feedme::ports::DisplayFrame&) {
    if (completed_ || released_) return;

    const uint32_t elapsed = millis() - startMs_;
    const float progress = (elapsed >= HOLD_DURATION_MS)
                               ? 1.0f
                               : static_cast<float>(elapsed) / HOLD_DURATION_MS;
    const int sweep = static_cast<int>(progress * ARC_SWEEP_MAX);
    if (sweep != lastSweep_) {
        lastSweep_ = sweep;
        lv_arc_set_value(fillArc_, sweep);
    }

    if (elapsed >= HOLD_DURATION_MS) {
        completed_ = true;
        // Destructive action no-op for v0 — wired up alongside whatever
        // schedule-mutation API lands in Phase D / Phase E.
        Serial.println("[lock] destructive confirm — clear today's schedule (no-op)");
        return;
    }

    // Release detection: if both physical inputs are no longer engaged,
    // the user let go before the threshold → cancel. Note: a 600 ms
    // long-press detector means the user must have been holding for at
    // least 600 ms when this view opened, so the first `stillHeld()`
    // poll one frame in should be true. Spurious "released on entry"
    // would point at a sensor bug, not user behaviour.
    if (!stillHeld()) {
        released_ = true;
    }
}

const char* LockConfirmView::handleInput(feedme::ports::TapEvent) {
    // No input routes here — the view is driven by polled press state,
    // not the discrete event stream. Re-firing LongPress while we're
    // already in LockConfirm is harmless and ignored.
    return nullptr;
}

const char* LockConfirmView::nextView() {
    if (completed_) return "idle";
    if (released_)  return returnTo_ ? returnTo_ : "idle";
    return nullptr;
}

}  // namespace feedme::views
