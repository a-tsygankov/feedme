#include "views/MenuView.h"

#include "views/Theme.h"

#include <Arduino.h>
#include <math.h>

namespace feedme::views {

namespace {

// Keep destinations + glyph chars co-indexed with selected_.
// LVGL doesn't ship dedicated bowl/clock/moon/gear icons, so v0 uses
// short text labels. Replace with proper line icons (per
// FeedMeKnobIcons.jsx) in a follow-up — see feedmeknob-plan.md
// Phase B postscript.
constexpr int    R_ORBIT  = 70;     // px from centre
constexpr int    GLYPH_PX = 40;     // glyph circle diameter

const char* const kLabels[MenuView::ITEM_COUNT] = {
    "Feed", "Schedule", "Quiet", "Settings",
};
const char* const kGlyphs[MenuView::ITEM_COUNT] = {
    "F",    "S",        "Q",     "G",
};
const char* const kDest[MenuView::ITEM_COUNT] = {
    "feedConfirm", "schedule", "quiet", "settings",
};

}  // namespace

void MenuView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Centre name of currently-highlighted item.
    centreLabel_ = lv_label_create(root_);
    lv_obj_set_style_text_color(centreLabel_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(centreLabel_, &lv_font_montserrat_24, 0);
    lv_label_set_text(centreLabel_, kLabels[0]);
    lv_obj_align(centreLabel_, LV_ALIGN_CENTER, 0, -8);

    // "press to open" hint below.
    centreHint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(centreHint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(centreHint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(centreHint_, "press to open");
    lv_obj_align(centreHint_, LV_ALIGN_CENTER, 0, 22);

    // Four orbit glyph circles, evenly spaced, starting at 12 o'clock.
    for (int i = 0; i < ITEM_COUNT; ++i) {
        const double ang = (-90.0 + (360.0 / ITEM_COUNT) * i) * M_PI / 180.0;
        const int cx = 120 + static_cast<int>(R_ORBIT * cos(ang));
        const int cy = 120 + static_cast<int>(R_ORBIT * sin(ang));

        glyphs_[i] = lv_obj_create(root_);
        lv_obj_set_size(glyphs_[i], GLYPH_PX, GLYPH_PX);
        lv_obj_set_style_radius(glyphs_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(glyphs_[i], 2, 0);
        lv_obj_set_style_border_color(glyphs_[i], lv_color_hex(kTheme.line), 0);
        lv_obj_set_style_bg_opa(glyphs_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(glyphs_[i], 0, 0);
        lv_obj_clear_flag(glyphs_[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        // LVGL aligns by widget centre when using LV_ALIGN_CENTER and
        // a relative offset. Translate from absolute (cx,cy) to offsets
        // from centre (120,120).
        lv_obj_align(glyphs_[i], LV_ALIGN_CENTER,
                     cx - 120, cy - 120);

        glyphLabels_[i] = lv_label_create(glyphs_[i]);
        lv_obj_set_style_text_color(glyphLabels_[i],
                                    lv_color_hex(kTheme.ink), 0);
        lv_obj_set_style_text_font(glyphLabels_[i], &lv_font_montserrat_18, 0);
        lv_label_set_text(glyphLabels_[i], kGlyphs[i]);
        lv_obj_center(glyphLabels_[i]);
    }
}

int MenuView::hitTest(int x, int y) const {
    // Glyph radius is GLYPH_PX/2; allow a small slack so finger-fat
    // taps near the edge still register. Sum of squared distances is
    // cheaper than sqrt and exact for circular hit-test.
    constexpr int RADIUS    = GLYPH_PX / 2 + 8;       // 28 px hit area
    constexpr int RADIUS_SQ = RADIUS * RADIUS;
    for (int i = 0; i < ITEM_COUNT; ++i) {
        const double ang = (-90.0 + (360.0 / ITEM_COUNT) * i) * M_PI / 180.0;
        const int cx = 120 + static_cast<int>(R_ORBIT * cos(ang));
        const int cy = 120 + static_cast<int>(R_ORBIT * sin(ang));
        const int dx = x - cx;
        const int dy = y - cy;
        if (dx * dx + dy * dy <= RADIUS_SQ) return i;
    }
    return -1;
}

void MenuView::applySelection() {
    for (int i = 0; i < ITEM_COUNT; ++i) {
        const bool sel = (i == selected_);
        if (sel) {
            lv_obj_set_style_border_color(glyphs_[i],
                lv_color_hex(kTheme.accent), 0);
            lv_obj_set_style_bg_color(glyphs_[i],
                lv_color_hex(kTheme.accent), 0);
            lv_obj_set_style_bg_opa(glyphs_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(glyphLabels_[i],
                lv_color_hex(kTheme.bg), 0);
        } else {
            lv_obj_set_style_border_color(glyphs_[i],
                lv_color_hex(kTheme.line), 0);
            lv_obj_set_style_bg_opa(glyphs_[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(glyphLabels_[i],
                lv_color_hex(kTheme.ink), 0);
        }
    }
    lv_label_set_text(centreLabel_, kLabels[selected_]);
}

void MenuView::onEnter() {
    selected_ = 0;
    applySelection();
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void MenuView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void MenuView::render(const feedme::ports::DisplayFrame&) {
    // Menu doesn't depend on FeedingState — nothing to refresh per tick.
}

const char* MenuView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::RotateCW:
            selected_ = (selected_ + 1) % ITEM_COUNT;
            applySelection();
            return nullptr;
        case E::RotateCCW:
            selected_ = (selected_ + ITEM_COUNT - 1) % ITEM_COUNT;
            applySelection();
            return nullptr;
        case E::Tap: {
            // Positional tap: if the touch coords are available AND
            // landed on one of the four glyphs, jump straight to that
            // item (skips the rotate-to-highlight step). Falls back
            // to "select the highlighted one" when coords aren't
            // available or the tap missed all glyphs.
            if (touch_) {
                const int x = touch_->lastTouchX();
                const int y = touch_->lastTouchY();
                if (x >= 0 && y >= 0) {
                    const int hit = hitTest(x, y);
                    if (hit >= 0) {
                        selected_ = hit;
                        applySelection();
                        Serial.printf("[menu] tap (%d,%d) → glyph %d (%s)\n",
                                      x, y, hit, kDest[hit]);
                        return kDest[hit];
                    }
                }
            }
            return kDest[selected_];
        }
        case E::Press:
            return kDest[selected_];
        case E::DoubleTap:
        case E::DoublePress:
            // Treat double-gestures as "go back to idle" for now —
            // less hostile than trapping the user inside the menu.
            return "idle";
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
