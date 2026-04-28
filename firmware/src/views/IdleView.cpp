#include "views/IdleView.h"

#include "assets/cats/cats.h"
#include "views/Theme.h"

#include <stdio.h>

namespace feedme::views {

namespace {

const lv_img_dsc_t* catForMood(feedme::domain::Mood m, bool small = false) {
    using M = feedme::domain::Mood;
    if (small) {
        switch (m) {
            case M::Happy:   return &cat_c2_88;
            case M::Neutral: return &cat_b1_88;
            case M::Warning:
            case M::Hungry:  return &cat_b2_88;
            case M::Sleepy:  return &cat_b3_88;
            case M::Fed:     return &cat_c4_88;
        }
        return &cat_b1_88;
    }
    switch (m) {
        case M::Happy:   return &cat_c2_130;
        case M::Neutral: return &cat_b1_130;
        case M::Warning:
        case M::Hungry:  return &cat_b2_130;
        case M::Sleepy:  return &cat_b3_130;
        case M::Fed:     return &cat_c4_130;
    }
    return &cat_b1_130;
}

}  // namespace

void IdleView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    timeLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(timeLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(timeLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(timeLbl_, "--:--");
    lv_obj_align(timeLbl_, LV_ALIGN_TOP_MID, 0, 32);

    kickerLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(kickerLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(kickerLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(kickerLbl_, "");
    lv_obj_align(kickerLbl_, LV_ALIGN_TOP_MID, 0, 70);

    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, catForMood(feedme::domain::Mood::Neutral));
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, 18);

    footerLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(footerLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(footerLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(footerLbl_, "next  13:00  lunch");
    lv_obj_align(footerLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void IdleView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    firstRender_ = true;  // force one full render after re-entry
}

void IdleView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void IdleView::render(const feedme::ports::DisplayFrame& frame) {
    const bool changed =
        firstRender_ ||
        frame.mood != lastFrame_.mood ||
        frame.minutesSinceFeed != lastFrame_.minutesSinceFeed ||
        frame.hour != lastFrame_.hour ||
        frame.minute != lastFrame_.minute;
    if (!changed) return;

    if (firstRender_ || frame.mood != lastFrame_.mood) {
        lv_img_set_src(catImg_, catForMood(frame.mood));
    }

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", frame.hour, frame.minute);
    lv_label_set_text(timeLbl_, timeBuf);

    char kickerBuf[24];
    if (frame.minutesSinceFeed < 0) {
        snprintf(kickerBuf, sizeof(kickerBuf), "no record");
    } else if (frame.minutesSinceFeed < 60) {
        snprintf(kickerBuf, sizeof(kickerBuf), "fed %dm ago",
                 frame.minutesSinceFeed);
    } else {
        snprintf(kickerBuf, sizeof(kickerBuf), "fed %dh %02dm ago",
                 frame.minutesSinceFeed / 60,
                 frame.minutesSinceFeed % 60);
    }
    lv_label_set_text(kickerLbl_, kickerBuf);

    lastFrame_ = frame;
    firstRender_ = false;
}

const char* IdleView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    // Per the FeedMeKnob FSM (handoff §3): tap or knob press → menu.
    // LongPress / LongTouch elsewhere routes to LockConfirm — handled by
    // the dispatcher in main.cpp, not here.
    if (ev == E::Tap || ev == E::Press) return "menu";
    return nullptr;
}

}  // namespace feedme::views
