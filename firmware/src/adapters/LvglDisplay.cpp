#include "adapters/LvglDisplay.h"

#include <Arduino.h>

namespace feedme::adapters {

namespace {

// Mood → palette. Tuned to match the React mockup.
struct Palette { uint32_t ring; const char* label; };

Palette paletteFor(feedme::domain::Mood m) {
    using M = feedme::domain::Mood;
    switch (m) {
        case M::Happy:   return {0x4ade80, "happy"};
        case M::Neutral: return {0xfacc15, "ok"};
        case M::Warning: return {0xfb923c, "soon"};
        case M::Hungry:  return {0xf87171, "FEED ME"};
        case M::Fed:     return {0x4ade80, "fed!"};
        case M::Sleepy:  return {0x818cf8, "zzz"};
    }
    return {0x4ade80, ""};
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
    // CrowPanel GC9A01 wants RGB565 bytes in LVGL's native order — passing
    // swap_bytes=true here turned dark navy (#1a1a24) into bright violet
    // and red (#f87171) into green. swap_bytes=false renders correctly.
    tft.pushColors(reinterpret_cast<uint16_t*>(&color_p->full), w * h, false);
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

    // TFT_eSPI's GC9A01 init sequence hard-codes BGR pixel order and
    // ignores TFT_RGB_ORDER for this driver. The CrowPanel's GC9A01
    // wants RGB. Force MADCTL bit 3 (BGR) to 0 by writing the register
    // directly. Byte 0x00 = no row/col flip, RGB order.
    tft.writecommand(0x36);  // MADCTL
    tft.writedata(0x00);     // MY=0 MX=0 MV=0 ML=0 BGR=0 MH=0

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
    lv_obj_set_size(arc_, 220, 220);
    lv_arc_set_rotation(arc_, 270);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_range(arc_, 0, 100);
    lv_obj_remove_style(arc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(arc_, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_, lv_color_hex(0x222), LV_PART_MAIN);
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
