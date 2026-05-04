#include "views/FeedConfirmView.h"

#include "assets/cats/CatSlug.h"
#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>
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

    // Kicker hint, faint uppercase. Uses the scrolling helper because
    // the multi-cat "ALL  CATS  -  PRESS POUR" line and long cat names
    // both overflow the chord at this y; LONG_DOT truncated them with
    // an ellipsis. LONG_SCROLL_CIRCULAR keeps the full text visible.
    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "TAP ADJ  PRESS POUR");
    applyScrollingLabel(hintLbl_, 160);
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);

    addBackHint(root_);
}

void FeedConfirmView::redraw() {
    if (!roster_ || roster_->count() == 0) return;
    const int sel = roster_->feedSelection();
    const bool feedAll = (sel == feedme::domain::CatRoster::FEED_ALL);

    // Pick the cat to display + the portion to show on the arc:
    //   - feedAll: use the first cat's slug as a placeholder hero,
    //     show TOTAL portion across all cats on the arc.
    //   - single cat: that cat's slug + portion.
    int displayedSlot;
    int g;  // grams to drive the arc + label
    if (feedAll) {
        displayedSlot = 0;
        g = 0;
        for (int i = 0; i < roster_->count(); ++i) {
            g += roster_->at(i).portion.grams();
        }
    } else {
        displayedSlot = sel;
        g = roster_->at(sel).portion.grams();
    }
    const auto& cat = roster_->at(displayedSlot);

    // Hero image — selected cat's slug (or first cat's for ALL).
    if (displayedSlot != lastDrawnActiveIdx_
        || strncmp(cat.slug, lastDrawnSlug_, 4) != 0) {
        lv_img_set_src(catImg_, feedme::assets::slugToPath(cat.slug, 88));
        strncpy(lastDrawnSlug_, cat.slug, 3);
        lastDrawnSlug_[3]   = '\0';
        lastDrawnActiveIdx_ = displayedSlot;
    }
    // Tint: a specific cat → that cat's color; FEED_ALL → white (the
    // image acts as a generic "all cats" placeholder, not a portrait).
    if (feedAll) {
        lv_obj_set_style_img_recolor_opa(catImg_, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_img_recolor(catImg_,
            lv_color_hex(cat.avatarColor), 0);
        lv_obj_set_style_img_recolor_opa(catImg_, LV_OPA_COVER, 0);
    }

    // Portion label — for ALL show the total grams; otherwise the
    // selected cat's portion. Arc still uses 0..MAX_G mapping for a
    // single cat; for ALL we cap the arc at full so the visual is
    // "whole arc, big number" rather than overflowing past 100%.
    char buf[12];
    if (feedAll) {
        snprintf(buf, sizeof(buf), "%d", g);
        lv_arc_set_value(arcFg_, ARC_SWEEP_MAX_DEG);
    } else {
        snprintf(buf, sizeof(buf), "%d", g);
        const int sweep = (g * ARC_SWEEP_MAX_DEG)
                          / feedme::domain::PortionState::MAX_G;
        lv_arc_set_value(arcFg_, sweep);
    }
    lv_label_set_text(portionLbl_, buf);

    // Hint line — show what the selection means + how to change it.
    // setScrollingText (matches applyScrollingLabel above) appends a
    // trailing gap so the wrap-around between loops is visible.
    //
    // Always include the cat's name when it's set (post user feedback:
    // earlier N==1 path showed the generic "TURN ADJ PRESS POUR" which
    // hid the user's chosen name). Multi-cat keeps the "ALL CATS"
    // affordance for the FEED_ALL selection.
    if (roster_->count() >= 2 && feedAll) {
        setScrollingText(hintLbl_, "ALL  CATS  -  PRESS POUR");
    } else if (cat.name[0] != '\0') {
        setScrollingText(hintLbl_, cat.name);
    } else {
        setScrollingText(hintLbl_, "TURN  ADJ  PRESS  POUR");
    }
    lastDrawnG_ = g;
}

void FeedConfirmView::onEnter() {
    lastDrawnG_         = -1;  // force a redraw
    lastDrawnActiveIdx_ = -1;
    lastDrawnSlug_[0]   = '\0';
    // Default selection: ALL for multi-cat households (the common
    // case — feed everyone at once), single-cat-0 for N=1.
    if (roster_ && roster_->count() >= 2) {
        roster_->setFeedSelection(feedme::domain::CatRoster::FEED_ALL);
    } else if (roster_ && roster_->count() == 1) {
        roster_->setFeedSelection(0);
    }
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
    const int N = roster_->count();
    switch (ev) {
        case E::RotateCW:
            if (N >= 2) {
                // Cycle selection through [ALL, 0, 1, ..., N-1, ALL...]
                const int sel = roster_->feedSelection();
                const int next = (sel == feedme::domain::CatRoster::FEED_ALL)
                                  ? 0
                                  : (sel + 1 >= N
                                     ? feedme::domain::CatRoster::FEED_ALL
                                     : sel + 1);
                roster_->setFeedSelection(next);
            } else {
                roster_->activePortion().bumpUp();
            }
            return nullptr;
        case E::RotateCCW:
            if (N >= 2) {
                const int sel = roster_->feedSelection();
                const int prev = (sel == feedme::domain::CatRoster::FEED_ALL)
                                  ? N - 1
                                  : (sel == 0
                                     ? feedme::domain::CatRoster::FEED_ALL
                                     : sel - 1);
                roster_->setFeedSelection(prev);
            } else {
                roster_->activePortion().bumpDown();
            }
            return nullptr;
        case E::Press: {
            // FEED_ALL fast path: skip the picker, use lastFeederIdx
            // (which currentFeederName resolves to in the absence of
            // an explicit picker selection). The user already picked
            // a feeder once — don't ask again on the "feed everyone"
            // shortcut. Single-cat or specific-cat selection still
            // routes through the picker for multi-user homes so the
            // user can change attribution per-cat if they want.
            const int sel = roster_->feedSelection();
            const bool feedAll = (sel == feedme::domain::CatRoster::FEED_ALL);
            const int uc = users_ ? users_->count() : -1;
            if (feedAll) {
                Serial.printf("[feedConfirm] Press FEED_ALL users=%d → pouring (last user)\n", uc);
                return "pouring";
            }
            Serial.printf("[feedConfirm] Press users=%d → %s\n",
                          uc, (uc >= 2) ? "feederPick" : "pouring");
            if (uc >= 2) return "feederPick";
            return "pouring";
        }
        case E::Tap: {
            // FEED_ALL + Tap → user selection (the only useful adjust;
            // there's no per-cat portion to edit in ALL mode since the
            // arc shows the SUM). N=1 user: no picker, no portion to
            // edit — stay put (return null).
            // Specific cat + Tap → portion adjuster (unchanged).
            const int sel = roster_->feedSelection();
            const bool feedAll = (sel == feedme::domain::CatRoster::FEED_ALL);
            if (feedAll) {
                if (users_ && users_->count() >= 2) return "feederPick";
                return nullptr;
            }
            return "portionAdjust";
        }
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:           return nullptr;
    }
}

}  // namespace feedme::views
