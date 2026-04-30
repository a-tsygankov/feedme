#include "adapters/LvglDisplay.h"

#include "views/Theme.h"

#include <Arduino.h>

#if !defined(SIMULATOR)
#  include "adapters/LvglLittleFs.h"
// LVGL's main header doesn't auto-include the PNG decoder — pull it
// in explicitly so lv_png_init() is declared.
#  include <src/extra/libs/png/lv_png.h>
#endif

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

#if !defined(SIMULATOR)
    // Register the PNG decoder with LVGL's image-decoder chain.
    // LV_USE_PNG=1 in lv_conf.h compiles the source in; this call
    // plugs it into the lookup so file extensions ending in .png
    // get routed to the pngle-based decoder. Without this, LVGL
    // falls back to "no data" rendering for any image source.
    lv_png_init();

    // Bridge LittleFS into LVGL so views can use "L:/cats/c2_88.png"
    // paths in lv_img_set_src. Must run before buildScene() because
    // lv_img_set_src eagerly opens the file to read header dimensions.
    // Requires LittleFS already mounted (main.cpp arranges this by
    // running storage.begin() before display.begin()).
    feedme::adapters::registerLvglLittleFs();
#endif

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
    // Wire shared portion state into the views that read/mutate it.
    // FeedingService is injected later (in main.cpp) once it exists.
    idleView_.setRoster(&roster_);
    feedConfirmView_.setRoster(&roster_);
    portionAdjustView_.setRoster(&roster_);
    pouringView_.setRoster(&roster_);
    scheduleView_.setRoster(&roster_);
    scheduleEditView_.setRoster(&roster_);
    quietView_.setQuiet(&quiet_);
    settingsView_.setQuiet(&quiet_);
    settingsView_.setWake(&wake_);
    wakeTimeEditView_.setWakeTime(&wake_);
    quietHoursEditView_.setQuiet(&quiet_);
    settingsView_.setRoster(&roster_);
    settingsView_.setUserRoster(&userRoster_);
    settingsView_.setTimeZone(&tz_);
    timezoneEditView_.setTimeZone(&tz_);
    catsListView_.setRoster(&roster_);
    catsListView_.setEditTarget(&catEditView_);
    catEditView_.setRoster(&roster_);
    catRemoveView_.setRoster(&roster_);
    usersListView_.setRoster(&userRoster_);
    feedConfirmView_.setUserRoster(&userRoster_);
    feederPickerView_.setRoster(&userRoster_);
    fedView_.setUserRoster(&userRoster_);
    fedView_.setCatRoster (&roster_);
    // settingsView_.setCoordinator() and thresholdEditView_.setCoordinator()
    // are called from main.cpp after DisplayCoordinator is constructed.
    // settingsView_.setNetwork() is called from main.cpp once network exists.

    screens_.begin(scr);
    screens_.registerView(&bootView_);
    screens_.registerView(&idleView_);
    screens_.registerView(&menuView_);
    screens_.registerView(&feedConfirmView_);
    screens_.registerView(&portionAdjustView_);
    screens_.registerView(&feederPickerView_);
    screens_.registerView(&pouringView_);
    screens_.registerView(&fedView_);
    screens_.registerView(&scheduleView_);
    screens_.registerView(&scheduleEditView_);
    screens_.registerView(&quietView_);
    screens_.registerView(&settingsView_);
    screens_.registerView(&lockConfirmView_);
    screens_.registerView(&wakeTimeEditView_);
    screens_.registerView(&timezoneEditView_);
    screens_.registerView(&quietHoursEditView_);
    screens_.registerView(&thresholdEditView_);
    screens_.registerView(&wifiResetView_);
    screens_.registerView(&catsListView_);
    screens_.registerView(&catEditView_);
    screens_.registerView(&catRemoveView_);
    screens_.registerView(&usersListView_);
    screens_.registerView(&setupView_);
#if defined(FEEDME_HAS_HOPPER)
    screens_.registerView(&hopperView_);
#endif
    // Power-on splash auto-advances to idle via BootView::nextView()
    // after BOOT_DURATION_MS.
    screens_.transition("boot");

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
        historyShownMs_ = millis();
        lv_obj_clear_flag(historyPanel_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(historyPanel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void LvglDisplay::render(const feedme::ports::DisplayFrame& frame) {
    screens_.render(frame);
}

void LvglDisplay::tick() {
    // Auto-dismiss the history overlay so it doesn't strand the user
    // if they wandered off after a double-tap. Cheap to check each
    // tick; setHistoryVisible(false) is idempotent.
    if (historyVisible_
        && (millis() - historyShownMs_) >= HISTORY_AUTO_DISMISS_MS) {
        setHistoryVisible(false);
    }
    lv_timer_handler();
}

}  // namespace feedme::adapters
