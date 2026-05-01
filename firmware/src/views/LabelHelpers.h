#pragma once

#include "views/Theme.h"

#include <lvgl.h>

#include <stdio.h>
#include <string.h>

namespace feedme::views {

// Two reusable label-configuration helpers, both addressing "long text
// on a 240×240 round screen". Every view that puts text near the top,
// bottom, or in a width-constrained slot was repeating the same 3-4
// line stanza of width-cap + long_mode + text-align. Centralised here.
//
// applyClippedLabel: width-capped + LV_LABEL_LONG_DOT + centred. Use
//   for short hint lines, single-value displays (SSID rows, IP, edit
//   hints) where truncation with an ellipsis is acceptable. Most
//   labels in the app use this.
//
// applyScrollingLabel: width-capped + LV_LABEL_LONG_SCROLL_CIRCULAR +
//   centred + slow anim speed. Use when the content is variable-length
//   and the user genuinely wants to read all of it (IdleView's
//   "<Cat> · fed Xm ago by <User>" combo, footer schedule line).
//
// `align` defaults to LV_TEXT_ALIGN_CENTER because nearly every call
// site centres; right-aligned values (Settings rows) pass
// LV_TEXT_ALIGN_RIGHT explicitly.
//
// `animSpeedPxPerSec` defaults to 25 — slow enough to read on a small
// round screen without feeling sluggish.
inline void applyClippedLabel(lv_obj_t* lbl,
                              int widthPx,
                              lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_set_width(lbl, widthPx);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(lbl, align, 0);
}

inline void applyScrollingLabel(lv_obj_t* lbl,
                                int widthPx,
                                int animSpeedPxPerSec = 25,
                                lv_text_align_t align = LV_TEXT_ALIGN_CENTER) {
    lv_obj_set_width(lbl, widthPx);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_speed(lbl, animSpeedPxPerSec, 0);
    lv_obj_set_style_text_align(lbl, align, 0);
}

// Trailing gap (in characters) appended by setScrollingText so the end
// of one scroll loop visibly separates from the start of the next.
// 4 spaces ≈ ~22 px at Montserrat 14 — about a finger-tip's worth of
// whitespace between "...by Andrey" and the next "Mochi · fed ...".
constexpr const char* kScrollingGap = "    ";

// Apply text to a scrolling label with the trailing gap baked in. Use
// this instead of lv_label_set_text directly when the label was
// configured via applyScrollingLabel — without the gap, the wrap-
// around looks like one continuous string ("by Andreyfed 3m ago").
//
// Buffer is sized for typical kicker / footer payloads (≤96 chars
// after the gap). Longer inputs are silently truncated by snprintf.
//
// IMPORTANT: dedups against the label's current text and skips the
// set_text call when nothing changed. Without this, a caller that
// re-fires every render frame (e.g. FeedConfirm::redraw) keeps
// reallocating the label buffer + invoking lv_label_refr_text, which
// resets LV_LABEL_LONG_SCROLL_CIRCULAR's animation back to position
// zero on every tick → label visibly never scrolls. Callers with
// their own change-detection still work — strcmp on a short hint
// string is essentially free.
inline void setScrollingText(lv_obj_t* lbl, const char* text) {
    if (!lbl) return;
    if (!text) text = "";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", text, kScrollingGap);
    const char* current = lv_label_get_text(lbl);
    if (current && strcmp(current, buf) == 0) return;
    lv_label_set_text(lbl, buf);
}

// Universal "long-press = back" hint. Every navigable view (anything
// with a meaningful parent()) should call this from build() so the
// gesture is discoverable. Sits at the very bottom edge of the round
// screen in the faint theme colour, below any per-view gesture hint
// at BOTTOM_MID -22.
//
// Default y=-4 puts it close to the bezel where the chord is narrow
// (~75 px). LV_SYMBOL_LEFT + " hold" measures ~50 px in Montserrat 14
// and reads as "◀ hold" — compact, no fancy font dependency.
//
// Returns the label object so callers can re-style or hide it for
// the rare view that needs to (e.g. the captive-portal SetupView,
// where there's no input at all and the hint would lie).
inline lv_obj_t* addBackHint(lv_obj_t* parent, int yOffset = -4) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_style_text_color(lbl, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT "  hold");
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, yOffset);
    return lbl;
}

}  // namespace feedme::views
