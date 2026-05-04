#include "views/IdleView.h"

#include "assets/cats/CatSlug.h"
#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <stdio.h>
#include <string.h>

namespace feedme::views {

namespace {

// Resolves to a LittleFS path string ("L:/cats/c2_130.png") via
// CatSlug.h's mood→slug + slug→path helpers. LVGL decodes on first
// display and caches the result.
inline const char* catForMood(feedme::domain::Mood m, bool small = false) {
    return feedme::assets::slugToPath(feedme::assets::moodToSlug(m),
                                      small ? 88 : 130);
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
    // Round-screen safe width: at y≈70 the chord across a 240 px circle
    // is ~218 px. Cap at 200; long combos like "Sebastian · fed 4h 23m
    // ago by Christopher" scroll instead of truncating.
    applyScrollingLabel(kickerLbl_, 200);
    lv_obj_align(kickerLbl_, LV_ALIGN_TOP_MID, 0, 70);

    catImg_ = lv_img_create(root_);
    lv_img_set_src(catImg_, catForMood(feedme::domain::Mood::Neutral));
    lv_obj_align(catImg_, LV_ALIGN_CENTER, 0, 18);

    footerLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(footerLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(footerLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(footerLbl_, "next  13:00  lunch");
    // At y≈218 (BOTTOM_MID -22) the chord is only ~138 px; long forms
    // like "next 22:00 breakfast" scroll instead of trimming.
    applyScrollingLabel(footerLbl_, 140);
    lv_obj_align(footerLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void IdleView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    firstRender_ = true;  // force one full render after re-entry
    lastDrawnActiveIdx_ = -1;
    lastDrawnRosterCount_ = -1;
}

void IdleView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void IdleView::render(const feedme::ports::DisplayFrame& frame) {
    const int activeIdx   = roster_ ? roster_->activeCatIdx() : -1;
    const int rosterCount = roster_ ? roster_->count()        : 0;

    const bool changed =
        firstRender_ ||
        frame.mood             != lastFrame_.mood ||
        frame.minutesSinceFeed != lastFrame_.minutesSinceFeed ||
        frame.hour             != lastFrame_.hour ||
        frame.minute           != lastFrame_.minute ||
        activeIdx              != lastDrawnActiveIdx_ ||
        rosterCount            != lastDrawnRosterCount_ ||
        strncmp(frame.lastFedBy, lastFrame_.lastFedBy,
                sizeof(frame.lastFedBy)) != 0;
    if (!changed) return;

    if (firstRender_ || frame.mood != lastFrame_.mood) {
        lv_img_set_src(catImg_, catForMood(frame.mood));
    }
    // Tint the silhouette with the active cat's avatar color. White
    // pixels in the PNG multiply with the recolor at COVER opacity →
    // tinted image, shading curves preserved.
    if (roster_ && rosterCount > 0) {
        const uint32_t tint = roster_->active().avatarColor;
        lv_obj_set_style_img_recolor(catImg_, lv_color_hex(tint), 0);
        lv_obj_set_style_img_recolor_opa(catImg_, LV_OPA_COVER, 0);
    } else {
        lv_obj_set_style_img_recolor_opa(catImg_, LV_OPA_TRANSP, 0);
    }

    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", frame.hour, frame.minute);
    lv_label_set_text(timeLbl_, timeBuf);

    // Kicker: "<Cat name> · fed Xm ago by Y" — always include the cat
    // name when it's available, regardless of roster count. Earlier
    // versions hid the name for N=1 households as an "adaptive UI"
    // simplification, but users who bothered to name their cat want
    // to see it (and per the user report, hiding it caused the
    // "main screen doesn't show cat's name" bug). Falls back to
    // "fed Xm ago" only when roster_ isn't wired or the active cat
    // has no name.
    char ageBuf[40];
    if (frame.minutesSinceFeed < 0) {
        snprintf(ageBuf, sizeof(ageBuf), "no record");
    } else if (frame.minutesSinceFeed < 60) {
        snprintf(ageBuf, sizeof(ageBuf), "fed %dm ago",
                 frame.minutesSinceFeed);
    } else {
        snprintf(ageBuf, sizeof(ageBuf), "fed %dh %02dm ago",
                 frame.minutesSinceFeed / 60,
                 frame.minutesSinceFeed % 60);
    }
    if (frame.minutesSinceFeed >= 0 && frame.lastFedBy[0] != '\0') {
        const size_t used = strlen(ageBuf);
        snprintf(ageBuf + used, sizeof(ageBuf) - used,
                 " by %s", frame.lastFedBy);
    }
    char kickerBuf[64];
    const char* catName = (roster_ && rosterCount >= 1) ? roster_->active().name : "";
    if (catName && catName[0] != '\0') {
        snprintf(kickerBuf, sizeof(kickerBuf), "%s  ·  %s", catName, ageBuf);
    } else {
        snprintf(kickerBuf, sizeof(kickerBuf), "%s", ageBuf);
    }
    setScrollingText(kickerLbl_, kickerBuf);

    // Footer derived from active cat's schedule. currentSlot wraps to
    // tomorrow's first meal once today's are all past — the JSX-style
    // "next HH:MM <label>" works either way.
    char footerBuf[32];
    if (roster_ && rosterCount > 0) {
        const auto& sched = roster_->activeSchedule();
        const int   slotIdx = sched.currentSlot(frame.hour);
        const auto& slot    = sched.slot(slotIdx);
        // Lower-case the label to match the design's footer style
        // ("next 13:00 lunch") — labels in MealSchedule are
        // capitalised ("Lunch"). 16-byte cap covers labels up to ~10.
        char labelLower[16];
        int j = 0;
        for (const char* p = slot.label; *p && j < (int)sizeof(labelLower) - 1; ++p, ++j) {
            char c = *p;
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            labelLower[j] = c;
        }
        labelLower[j] = '\0';
        snprintf(footerBuf, sizeof(footerBuf), "next  %02d:00  %s",
                 slot.hour, labelLower);
    } else {
        snprintf(footerBuf, sizeof(footerBuf), "next  -");
    }
    setScrollingText(footerLbl_, footerBuf);

    lastFrame_            = frame;
    lastDrawnActiveIdx_   = activeIdx;
    lastDrawnRosterCount_ = rosterCount;
    firstRender_          = false;
}

const char* IdleView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    // Per the FeedMeKnob FSM (handoff §3): tap or knob press → menu.
    // LongPress / LongTouch elsewhere routes to LockConfirm — handled by
    // the dispatcher in main.cpp, not here.
    if (ev == E::Tap || ev == E::Press) return "menu";

    // Cat selector — rotate cycles activeCatIdx when N>=2. Adaptive UI
    // rule: with N=1 there's no choice to make, so rotate is inert.
    if (roster_ && roster_->count() >= 2) {
        const int N = roster_->count();
        const int cur = roster_->activeCatIdx();
        if (ev == E::RotateCW) {
            roster_->setActiveCatIdx((cur + 1) % N);
            return nullptr;
        }
        if (ev == E::RotateCCW) {
            roster_->setActiveCatIdx((cur + N - 1) % N);
            return nullptr;
        }
    }
    return nullptr;
}

}  // namespace feedme::views
