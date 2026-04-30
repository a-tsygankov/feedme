#include "views/QuietView.h"

#include "views/Theme.h"

#include <math.h>
#include <stdio.h>

namespace feedme::views {

namespace {

constexpr int   ARC_RADIUS_PX = 105;
constexpr int   ARC_DIAM_PX   = ARC_RADIUS_PX * 2;
constexpr int   NOW_TICK_R    = 100;        // perimeter radius for the dot
constexpr int   NOW_TICK_DIAM = 8;
// LVGL rotation 270° puts arc 0° at 12 o'clock; combined with the
// per-hour rotation below it places the wedge correctly.
constexpr int   DIAL_ROTATION = 270;

// 360° / 24h = 15° per hour = 0.25° per minute.
constexpr float DEG_PER_MINUTE = 0.25f;

// Wedge geometry from live start/end values.
inline float quietStartDeg(int startHour, int startMinute) {
    return (startHour * 60 + startMinute) * DEG_PER_MINUTE;
}
inline float quietSweepDeg(int startHour, int startMinute,
                           int endHour,   int endMinute) {
    const int startMin = startHour * 60 + startMinute;
    const int endMin   = endHour   * 60 + endMinute;
    const int span = (endMin >= startMin)
                     ? (endMin - startMin)
                     : (24 * 60 - startMin + endMin);
    return span * DEG_PER_MINUTE;
}

}  // namespace

void QuietView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // 24-h thin dial ring, full circle.
    dialArc_ = lv_arc_create(root_);
    lv_obj_set_size(dialArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(dialArc_);
    lv_arc_set_rotation(dialArc_, DIAL_ROTATION);
    lv_arc_set_bg_angles(dialArc_, 0, 359);
    lv_obj_remove_style(dialArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(dialArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(dialArc_, lv_color_hex(kTheme.line), LV_PART_MAIN);
    lv_obj_set_style_arc_width(dialArc_, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(dialArc_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Quiet wedge — geometry is recomputed in render() when start/end
    // change, so build() leaves it at default (0..0) until the first
    // redraw populates it from the live QuietWindow values.
    quietArc_ = lv_arc_create(root_);
    lv_obj_set_size(quietArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(quietArc_);
    lv_arc_set_rotation(quietArc_, DIAL_ROTATION);
    lv_arc_set_bg_angles(quietArc_, 0, 0);
    lv_obj_remove_style(quietArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(quietArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(quietArc_, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(quietArc_, LV_OPA_TRANSP, LV_PART_INDICATOR);
    // Colour set in applyEnabledStyles(); geometry in applyTimeStyles().

    // Now-tick (small filled circle on the perimeter at current minute).
    nowTick_ = lv_obj_create(root_);
    lv_obj_set_size(nowTick_, NOW_TICK_DIAM, NOW_TICK_DIAM);
    lv_obj_set_style_radius(nowTick_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(nowTick_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_bg_opa(nowTick_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nowTick_, 0, 0);
    lv_obj_set_style_pad_all(nowTick_, 0, 0);
    lv_obj_clear_flag(nowTick_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // Centre title — design wants a moon glyph above it; LVGL ships no
    // moon symbol so the text label carries the meaning for now.
    // Replace with a custom IcMoon image asset in a follow-up.
    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(titleLbl_, "Quiet");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, -16);

    rangeLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(rangeLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(rangeLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(rangeLbl_, "--:-- - --:--");  // populated in redraw()
    lv_obj_align(rangeLbl_, LV_ALIGN_CENTER, 0, 14);

    statusLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_font(statusLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(statusLbl_, "");
    lv_obj_align(statusLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void QuietView::applyEnabledStyles() {
    if (!quiet_) return;
    const bool en = quiet_->enabled();
    const uint32_t wedgeColor = en ? kTheme.accent : kTheme.line;
    const lv_opa_t wedgeOpa   = en ? LV_OPA_COVER  : LV_OPA_50;
    lv_obj_set_style_arc_color(quietArc_, lv_color_hex(wedgeColor), LV_PART_MAIN);
    lv_obj_set_style_arc_opa  (quietArc_, wedgeOpa, LV_PART_MAIN);

    if (en) {
        lv_label_set_text(statusLbl_, "PAUSED");
        lv_obj_set_style_text_color(statusLbl_,
                                    lv_color_hex(kTheme.accent), 0);
    } else {
        lv_label_set_text(statusLbl_, "TAP  ENABLE");
        lv_obj_set_style_text_color(statusLbl_,
                                    lv_color_hex(kTheme.faint), 0);
    }
}

void QuietView::applyTimeStyles() {
    if (!quiet_) return;
    const int sh = quiet_->startHour();
    const int sm = quiet_->startMinute();
    const int eh = quiet_->endHour();
    const int em = quiet_->endMinute();
    const int rotation = (DIAL_ROTATION
                          + static_cast<int>(quietStartDeg(sh, sm))) % 360;
    const int sweep    = static_cast<int>(quietSweepDeg(sh, sm, eh, em));
    lv_arc_set_rotation(quietArc_, rotation);
    lv_arc_set_bg_angles(quietArc_, 0, sweep);

    char rangeBuf[16];
    snprintf(rangeBuf, sizeof(rangeBuf), "%02d:%02d - %02d:%02d", sh, sm, eh, em);
    lv_label_set_text(rangeLbl_, rangeBuf);
}

void QuietView::positionNowTick(int hour, int minute) {
    const float minutesToday = hour * 60 + minute;
    const float angleCwFrom12 = minutesToday * DEG_PER_MINUTE;
    const float mathRad = (angleCwFrom12 - 90.0f) * static_cast<float>(M_PI) / 180.0f;
    const int x = 120 + static_cast<int>(NOW_TICK_R * cosf(mathRad));
    const int y = 120 + static_cast<int>(NOW_TICK_R * sinf(mathRad));
    lv_obj_align(nowTick_, LV_ALIGN_TOP_LEFT,
                 x - NOW_TICK_DIAM / 2, y - NOW_TICK_DIAM / 2);
}

void QuietView::redraw(int hour, int minute) {
    if (!quiet_) return;

    const bool enabledChanged = firstRender_
                                || quiet_->enabled() != lastDrawnEnabled_;
    const bool tickChanged = firstRender_
                             || hour   != lastDrawnHour_
                             || minute != lastDrawnMinute_;
    const bool timesChanged = firstRender_
                              || quiet_->startHour()   != lastDrawnStartHour_
                              || quiet_->startMinute() != lastDrawnStartMinute_
                              || quiet_->endHour()     != lastDrawnEndHour_
                              || quiet_->endMinute()   != lastDrawnEndMinute_;
    if (!enabledChanged && !tickChanged && !timesChanged) return;

    if (timesChanged) {
        applyTimeStyles();
        lastDrawnStartHour_   = quiet_->startHour();
        lastDrawnStartMinute_ = quiet_->startMinute();
        lastDrawnEndHour_     = quiet_->endHour();
        lastDrawnEndMinute_   = quiet_->endMinute();
    }
    if (enabledChanged) {
        applyEnabledStyles();
        lastDrawnEnabled_ = quiet_->enabled();
    }
    if (tickChanged) {
        positionNowTick(hour, minute);
        lastDrawnHour_   = hour;
        lastDrawnMinute_ = minute;
    }
    firstRender_ = false;
}

void QuietView::onEnter() {
    firstRender_      = true;
    lastDrawnHour_    = -1;
    lastDrawnMinute_  = -1;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void QuietView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void QuietView::render(const feedme::ports::DisplayFrame& frame) {
    redraw(frame.hour, frame.minute);
}

const char* QuietView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (!quiet_) return nullptr;
    switch (ev) {
        case E::Tap:
        case E::Press:
            quiet_->toggle();
            return nullptr;
        // Rotate enters the start/end times editor. Used to live behind
        // Settings → Quiet, but that row was removed (it duplicated the
        // F-S-Q-G menu's Q glyph). Either rotate direction works — both
        // mean "configure" rather than the editor's own per-field nudge.
        case E::RotateCW:
        case E::RotateCCW:
            return "quietHoursEdit";
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
