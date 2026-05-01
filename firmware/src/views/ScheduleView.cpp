#include "views/ScheduleView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <math.h>
#include <stdio.h>

namespace feedme::views {

namespace {

constexpr int   SLOT_RADIUS_PX = 88;   // distance from centre
constexpr int   SLOT_DIAM_PX   = 44;   // circle diameter
// 12 / 3 / 6 / 9 o'clock = -90° / 0° / 90° / 180°.
constexpr float SLOT_ANGLES_DEG[] = { -90.0f, 0.0f, 90.0f, 180.0f };

}  // namespace

void ScheduleView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Centre: selected slot's time + label.
    centreTime_ = lv_label_create(root_);
    lv_obj_set_style_text_color(centreTime_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(centreTime_, &lv_font_montserrat_24, 0);
    lv_label_set_text(centreTime_, "--:--");
    lv_obj_align(centreTime_, LV_ALIGN_CENTER, 0, -8);

    centreLabel_ = lv_label_create(root_);
    lv_obj_set_style_text_color(centreLabel_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(centreLabel_, &lv_font_montserrat_14, 0);
    lv_label_set_text(centreLabel_, "");
    lv_obj_align(centreLabel_, LV_ALIGN_CENTER, 0, 22);

    // Four slot circles around the perimeter.
    for (int i = 0; i < feedme::domain::MealSchedule::SLOT_COUNT; ++i) {
        const float ang = SLOT_ANGLES_DEG[i] * static_cast<float>(M_PI) / 180.0f;
        const int dx = static_cast<int>(SLOT_RADIUS_PX * cosf(ang));
        const int dy = static_cast<int>(SLOT_RADIUS_PX * sinf(ang));

        slotCircles_[i] = lv_obj_create(root_);
        lv_obj_set_size(slotCircles_[i], SLOT_DIAM_PX, SLOT_DIAM_PX);
        lv_obj_set_style_radius(slotCircles_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(slotCircles_[i], 2, 0);
        lv_obj_set_style_pad_all(slotCircles_[i], 0, 0);
        lv_obj_clear_flag(slotCircles_[i],
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(slotCircles_[i], LV_ALIGN_CENTER, dx, dy);

        slotTimes_[i] = lv_label_create(slotCircles_[i]);
        lv_obj_set_style_text_font(slotTimes_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(slotTimes_[i], "--");
        lv_obj_align(slotTimes_[i], LV_ALIGN_CENTER, 0, -6);

        slotSubs_[i] = lv_label_create(slotCircles_[i]);
        lv_obj_set_style_text_font(slotSubs_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(slotSubs_[i], "");
        lv_obj_align(slotSubs_[i], LV_ALIGN_CENTER, 0, 8);
    }

    addBackHint(root_);
}

void ScheduleView::applySlotStyles(int currentIdx) {
    if (!roster_ || roster_->count() == 0) return;
    const auto& schedule = roster_->activeSchedule();
    for (int i = 0; i < feedme::domain::MealSchedule::SLOT_COUNT; ++i) {
        const bool served = (lastDrawnHour_ >= 0)
                            && schedule.isServed(i, lastDrawnHour_);
        const bool isNow  = (i == currentIdx);

        // Border tracks "now"; fill tracks "served"; selection is shown
        // by a thicker border (3 vs 2) so the user can see their scrub.
        const uint32_t borderHex = isNow ? kTheme.accent : kTheme.line;
        lv_obj_set_style_border_color(slotCircles_[i],
                                      lv_color_hex(borderHex), 0);
        lv_obj_set_style_border_width(slotCircles_[i],
                                      (i == selectedIdx_) ? 3 : 2, 0);

        if (served) {
            lv_obj_set_style_bg_color(slotCircles_[i],
                                      lv_color_hex(kTheme.accent), 0);
            lv_obj_set_style_bg_opa(slotCircles_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(slotTimes_[i],
                                        lv_color_hex(kTheme.bg), 0);
            lv_obj_set_style_text_color(slotSubs_[i],
                                        lv_color_hex(kTheme.bg), 0);
            lv_label_set_text(slotSubs_[i], LV_SYMBOL_OK);
        } else {
            lv_obj_set_style_bg_opa(slotCircles_[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(slotTimes_[i],
                                        lv_color_hex(kTheme.ink), 0);
            lv_obj_set_style_text_color(slotSubs_[i],
                                        lv_color_hex(kTheme.dim), 0);
            lv_label_set_text(slotSubs_[i], isNow ? "now" : "");
        }
    }
}

void ScheduleView::redraw(int hour) {
    if (!roster_ || roster_->count() == 0) return;
    const auto& schedule = roster_->activeSchedule();
    const int currentIdx = schedule.currentSlot(hour);
    if (selectedIdx_ < 0) selectedIdx_ = currentIdx;

    if (hour == lastDrawnHour_ && selectedIdx_ == lastDrawnSelected_) return;
    lastDrawnHour_     = hour;
    lastDrawnSelected_ = selectedIdx_;

    // Slot times only need printing once but it's cheap to do here so
    // build() doesn't need to know about the schedule pointer.
    for (int i = 0; i < feedme::domain::MealSchedule::SLOT_COUNT; ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02d", schedule.slot(i).hour);
        lv_label_set_text(slotTimes_[i], buf);
    }

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:00",
             schedule.slot(selectedIdx_).hour);
    lv_label_set_text(centreTime_, timeBuf);
    lv_label_set_text(centreLabel_, schedule.slot(selectedIdx_).label);

    applySlotStyles(currentIdx);
}

void ScheduleView::onEnter() {
    selectedIdx_       = -1;  // "default to current" on first render
    lastDrawnHour_     = -1;
    lastDrawnSelected_ = -1;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ScheduleView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void ScheduleView::render(const feedme::ports::DisplayFrame& frame) {
    redraw(frame.hour);
}

const char* ScheduleView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    constexpr int N = feedme::domain::MealSchedule::SLOT_COUNT;
    switch (ev) {
        case E::RotateCW:
            if (selectedIdx_ >= 0) selectedIdx_ = (selectedIdx_ + 1) % N;
            return nullptr;
        case E::RotateCCW:
            if (selectedIdx_ >= 0) selectedIdx_ = (selectedIdx_ + N - 1) % N;
            return nullptr;
        case E::Tap:
            return "idle";
        case E::Press:
            // Press on the knob enters the schedule editor for the
            // active cat. Tap (touch) keeps the canonical "back to
            // idle" behaviour from the gesture map.
            return "scheduleEdit";
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
