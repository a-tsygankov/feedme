#include "views/ScheduleEditView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

void ScheduleEditView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    headerLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(headerLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(headerLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(headerLbl_, "MEAL  TIMES");
    lv_obj_align(headerLbl_, LV_ALIGN_TOP_MID, 0, 36);

    slotLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(slotLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(slotLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(slotLbl_, "Breakfast");
    lv_obj_align(slotLbl_, LV_ALIGN_TOP_MID, 0, 60);

    // Big "07" with smaller ":00" unit. Hour-only resolution keeps the
    // editor simple — meal times are commonly hourly anyway.
    hourLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hourLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(hourLbl_, &lv_font_montserrat_48, 0);
    lv_label_set_text(hourLbl_, "07");
    lv_obj_align(hourLbl_, LV_ALIGN_CENTER, -18, 6);

    unitLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(unitLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(unitLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(unitLbl_, ":00");
    lv_obj_align(unitLbl_, LV_ALIGN_CENTER, 30, 14);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "TURN ADJ  PRESS NEXT");
    applyClippedLabel(hintLbl_, 160);
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -24);
}

void ScheduleEditView::redraw() {
    if (!roster_ || roster_->count() == 0) return;
    const auto& sched = roster_->activeSchedule();
    const int hour = sched.slotHour(focusSlot_);
    if (focusSlot_ == lastDrawnSlot_
        && hour == lastDrawnHour_
        && !firstRender_) return;

    lv_label_set_text(slotLbl_, sched.slot(focusSlot_).label);
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", hour);
    lv_label_set_text(hourLbl_, buf);

    lastDrawnSlot_ = focusSlot_;
    lastDrawnHour_ = hour;
    firstRender_   = false;
}

void ScheduleEditView::onEnter() {
    focusSlot_     = 0;
    firstRender_   = true;
    lastDrawnSlot_ = -1;
    lastDrawnHour_ = -1;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ScheduleEditView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ScheduleEditView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* ScheduleEditView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!roster_ || roster_->count() == 0) return nullptr;

    switch (ev) {
        case E::RotateCW:  roster_->bumpActiveSlotHour(focusSlot_, +1); return nullptr;
        case E::RotateCCW: roster_->bumpActiveSlotHour(focusSlot_, -1); return nullptr;
        case E::Tap:
        case E::Press:
            ++focusSlot_;
            if (focusSlot_ >= feedme::domain::MealSchedule::SLOT_COUNT) {
                // Final press → save (already mutating through bumpActiveSlotHour;
                // main.cpp persists on roster.consumeDirty()) → back to view.
                return "schedule";
            }
            return nullptr;
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
