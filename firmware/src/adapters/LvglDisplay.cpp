#include "adapters/LvglDisplay.h"

#include <Arduino.h>

namespace feedme::adapters {

namespace {

// Mood → palette. Tuned to match the React mockup.
struct Palette { uint32_t ring; const char* label; };

Palette paletteFor(feedme::domain::Mood m) {
    using M = feedme::domain::Mood;
    // Brighter / more saturated than the React mockup palette so the
    // 9-px ring stays legible against the dark navy background on
    // the round panel. Matched in pairs across mood transitions.
    switch (m) {
        case M::Happy:   return {0x22ff66, "happy"};
        case M::Neutral: return {0xffea00, "ok"};
        case M::Warning: return {0xff9100, "soon"};
        case M::Hungry:  return {0xff2a2a, "FEED ME"};
        case M::Fed:     return {0x22ff66, "fed!"};
        case M::Sleepy:  return {0x6e7bff, "zzz"};
    }
    return {0x22ff66, ""};
}

// ── LVGL display flush wired to TFT_eSPI ──────────────────────────────────
constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 240;
constexpr int BUF_LINES = 20;

TFT_eSPI tft;
lv_disp_draw_buf_t draw_buf;
lv_color_t buf1[SCREEN_W * BUF_LINES];

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    const uint32_t w = area->x2 - area->x1 + 1;
    const uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // LV_COLOR_16_SWAP=0 means our buffer holds standard RGB565 in
    // CPU-native (little-endian) byte order. The GC9A01 wants
    // big-endian RGB565 on the wire, so let TFT_eSPI do the byte swap.
    tft.pushColors(reinterpret_cast<uint16_t*>(&color_p->full), w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

// LVGL ticks come from LV_TICK_CUSTOM in lv_conf.h (reads millis() inline).

}  // namespace

void LvglDisplay::begin() {
    // LCD power rail is enabled by main.cpp before Serial.begin().
    // Backlight (GPIO 46) is driven by TFT_eSPI itself when TFT_BL is
    // defined and TFT_BACKLIGHT_ON=HIGH.

    tft.init();
    tft.setRotation(0);
    // R↔B channel swap is performed in flushCb (the panel reads BGR
    // even after MADCTL overrides), so no need to fight TFT_eSPI's
    // GC9A01 init sequence here.
    tft.fillScreen(TFT_BLACK);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, nullptr, SCREEN_W * BUF_LINES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_W;
    disp_drv.ver_res = SCREEN_H;
    disp_drv.flush_cb = flushCb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    buildScene();
}

void LvglDisplay::buildScene() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f14), 0);

    arc_ = lv_arc_create(scr);
    lv_obj_set_size(arc_, 230, 230);
    lv_arc_set_rotation(arc_, 270);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_range(arc_, 0, 100);
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    // Wider ring (14 px) with brighter unfilled-track grey for visible
    // contrast against the dark background.
    lv_obj_set_style_arc_width(arc_, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x333344), LV_PART_MAIN);
    lv_obj_center(arc_);

    // Inner background panel (still a dark circle — gives the cat a backdrop
    // distinct from the surrounding screen and matches the mockup's surface).
    face_ = lv_obj_create(scr);
    lv_obj_set_size(face_, 200, 200);
    lv_obj_set_style_radius(face_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(face_, 0, 0);
    lv_obj_set_style_bg_color(face_, lv_color_hex(0x1a1a24), 0);
    lv_obj_set_style_pad_all(face_, 0, 0);
    lv_obj_clear_flag(face_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(face_);

    // Simon's Cat-style face — primitive widgets, mood-driven.
    cat_.begin(face_);
    cat_.align(LV_ALIGN_CENTER, 0, -25);

    moodLbl_ = lv_label_create(face_);
    lv_obj_set_style_text_color(moodLbl_, lv_color_white(), 0);
    lv_obj_set_style_text_font(moodLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(moodLbl_, "...");
    lv_obj_align(moodLbl_, LV_ALIGN_CENTER, 0, 38);

    timeLbl_ = lv_label_create(face_);
    lv_obj_set_style_text_color(timeLbl_, lv_color_hex(0x888), 0);
    lv_obj_set_style_text_font(timeLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(timeLbl_, "");
    lv_obj_align(timeLbl_, LV_ALIGN_CENTER, 0, 60);

    for (int i = 0; i < 3; ++i) {
        dots_[i] = lv_obj_create(scr);
        lv_obj_set_size(dots_[i], 10, 10);
        lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dots_[i], 0, 0);
        lv_obj_set_style_bg_color(dots_[i], lv_color_hex(0x444), 0);
        lv_obj_clear_flag(dots_[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(dots_[i], LV_ALIGN_BOTTOM_MID, (i - 1) * 16, -16);
    }

    buildHistoryOverlay();
}

void LvglDisplay::buildHistoryOverlay() {
    lv_obj_t* scr = lv_scr_act();

    // Full-screen panel that covers the cat scene when visible.
    historyPanel_ = lv_obj_create(scr);
    lv_obj_set_size(historyPanel_, 240, 240);
    lv_obj_center(historyPanel_);
    lv_obj_set_style_radius(historyPanel_, 0, 0);
    lv_obj_set_style_bg_color(historyPanel_, lv_color_hex(0x0f0f14), 0);
    lv_obj_set_style_bg_opa(historyPanel_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(historyPanel_, 0, 0);
    lv_obj_set_style_pad_all(historyPanel_, 8, 0);
    lv_obj_clear_flag(historyPanel_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(historyPanel_, LV_OBJ_FLAG_HIDDEN);

    historyTitle_ = lv_label_create(historyPanel_);
    lv_obj_set_style_text_color(historyTitle_, lv_color_white(), 0);
    lv_obj_set_style_text_font(historyTitle_, &lv_font_montserrat_18, 0);
    lv_label_set_text(historyTitle_, "history");
    lv_obj_align(historyTitle_, LV_ALIGN_TOP_MID, 0, 30);

    for (int i = 0; i < HISTORY_MAX; ++i) {
        historyLines_[i] = lv_label_create(historyPanel_);
        lv_obj_set_style_text_color(historyLines_[i], lv_color_hex(0xc0c0d0), 0);
        lv_obj_set_style_text_font(historyLines_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(historyLines_[i], "");
        lv_obj_align(historyLines_[i], LV_ALIGN_TOP_MID, 0, 70 + i * 20);
    }
}

void LvglDisplay::setHistory(const HistoryItem* items, int count) {
    if (count > HISTORY_MAX) count = HISTORY_MAX;
    for (int i = 0; i < HISTORY_MAX; ++i) {
        const char* text = (i < count) ? items[i].line : "";
        lv_label_set_text(historyLines_[i], text);
    }
    if (count == 0) {
        lv_label_set_text(historyLines_[0], "(no events yet)");
    }
}

void LvglDisplay::setHistoryVisible(bool visible) {
    historyVisible_ = visible;
    if (visible) {
        lv_obj_clear_flag(historyPanel_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(historyPanel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void LvglDisplay::render(const feedme::ports::DisplayFrame& frame) {
    const bool changed =
        firstRender_ ||
        frame.mood != lastFrame_.mood ||
        frame.todayCount != lastFrame_.todayCount ||
        frame.minutesSinceFeed != lastFrame_.minutesSinceFeed ||
        static_cast<int>(frame.ringProgress * 100) !=
            static_cast<int>(lastFrame_.ringProgress * 100);
    if (!changed) return;

    const auto p = paletteFor(frame.mood);

    lv_arc_set_value(arc_, static_cast<int>(frame.ringProgress * 100));
    lv_obj_set_style_arc_color(arc_, lv_color_hex(p.ring), LV_PART_INDICATOR);

    if (frame.mood != lastFrame_.mood || firstRender_) {
        cat_.setMood(frame.mood);
    }

    lv_label_set_text(moodLbl_, p.label);
    lv_obj_set_style_text_color(moodLbl_, lv_color_hex(p.ring), 0);

    char buf[24];
    if (frame.minutesSinceFeed < 0) {
        snprintf(buf, sizeof(buf), "no record");
    } else if (frame.minutesSinceFeed < 60) {
        snprintf(buf, sizeof(buf), "%dm ago", frame.minutesSinceFeed);
    } else {
        snprintf(buf, sizeof(buf), "%dh %dm ago",
                 frame.minutesSinceFeed / 60, frame.minutesSinceFeed % 60);
    }
    lv_label_set_text(timeLbl_, buf);

    for (int i = 0; i < 3; ++i) {
        const bool filled = i < frame.todayCount;
        lv_obj_set_style_bg_color(dots_[i],
            filled ? lv_color_hex(p.ring) : lv_color_hex(0x444), 0);
    }

    lastFrame_ = frame;
    firstRender_ = false;
}

void LvglDisplay::tick() {
    lv_timer_handler();
}

}  // namespace feedme::adapters
