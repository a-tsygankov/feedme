#pragma once

#include <lvgl.h>

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

}  // namespace feedme::views
