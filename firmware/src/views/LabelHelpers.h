#pragma once

#include <lvgl.h>

#include <stdio.h>

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
inline void setScrollingText(lv_obj_t* lbl, const char* text) {
    if (!lbl) return;
    if (!text) text = "";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", text, kScrollingGap);
    lv_label_set_text(lbl, buf);
}

}  // namespace feedme::views
