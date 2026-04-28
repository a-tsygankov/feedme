#include "adapters/LvglDisplay.h"

#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::adapters {

namespace {

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
    using namespace feedme::views;
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(kTheme.bg), 0);

    // ScreenManager owns the per-view widget hierarchies.
    screens_.begin(scr);
    screens_.registerView(&idleView_);
    screens_.registerView(&menuView_);
    screens_.transition("idle");

    // Compile the legacy primitive cat without putting it on the screen.
    // (Open question 3 in docs/feedmeknob-plan.md, answered "keep".)
    (void)cat_;

    buildHistoryOverlay();
}

const char* LvglDisplay::handleInput(feedme::ports::TapEvent ev) {
    const char* next = screens_.handleInput(ev);
    if (next) screens_.transition(next);
    return next;
}

void LvglDisplay::transitionTo(const char* viewName) {
    screens_.transition(viewName);
}

const char* LvglDisplay::currentView() const {
    auto* v = screens_.current();
    return v ? v->name() : nullptr;
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
    screens_.render(frame);
}

void LvglDisplay::tick() {
    lv_timer_handler();
}

}  // namespace feedme::adapters
