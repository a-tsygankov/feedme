#include "views/WifiResetView.h"

#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {

// Map RSSI (dBm) to signal-bar count 0..4. Common ESP32 thresholds:
//   >= -55 → excellent (4)
//   >= -65 → good      (3)
//   >= -75 → fair      (2)
//   >= -85 → weak      (1)
//   else / not associated → 0
int rssiToBars(int rssi) {
    if (rssi == 0)    return 0;   // adapter says "unknown" / offline
    if (rssi >= -55)  return 4;
    if (rssi >= -65)  return 3;
    if (rssi >= -75)  return 2;
    if (rssi >= -85)  return 1;
    return 0;
}

}  // namespace

void WifiResetView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    iconLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(iconLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(iconLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(iconLbl_, LV_SYMBOL_WIFI);
    lv_obj_align(iconLbl_, LV_ALIGN_TOP_MID, 0, 30);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Switch Wi-Fi");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 56);

    // Current-connection info stack. Refreshed on every onEnter so the
    // numbers reflect "right now", not stale state from a prior visit.
    ssidLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(ssidLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(ssidLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(ssidLbl_, "");
    lv_obj_set_width(ssidLbl_, 200);
    lv_label_set_long_mode(ssidLbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ssidLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ssidLbl_, LV_ALIGN_TOP_MID, 0, 96);

    ipLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(ipLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(ipLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(ipLbl_, "");
    lv_obj_set_width(ipLbl_, 200);
    lv_label_set_long_mode(ipLbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ipLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ipLbl_, LV_ALIGN_TOP_MID, 0, 122);

    // Signal: 4 bars rendered with U+2588 (full block) — Montserrat
    // ships full-block glyphs as part of the symbol set used for
    // progress indicators. Inactive bars use the dim theme colour;
    // active ones the accent. Plus a numeric "-65 dBm" suffix.
    rssiLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(rssiLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(rssiLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_recolor(rssiLbl_, true);   // enable per-segment recoloring
    lv_label_set_text(rssiLbl_, "");
    lv_obj_set_width(rssiLbl_, 200);
    lv_obj_set_style_text_align(rssiLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(rssiLbl_, LV_ALIGN_TOP_MID, 0, 146);

    // bodyLbl repurposed: was "device will reboot"; in-place flow
    // doesn't reboot. Now a tiny "press to switch" hint above the
    // gesture line.
    bodyLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(bodyLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(bodyLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(bodyLbl_, "tap to switch");
    lv_obj_align(bodyLbl_, LV_ALIGN_BOTTOM_MID, 0, -50);

    hint_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hint_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hint_, "PRESS  OK   TURN  X");
    lv_obj_set_width(hint_, 140);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void WifiResetView::onEnter() {
    // Snapshot current connection info so the user sees what they're
    // switching FROM. Reads INetwork directly — adapter reports empty
    // strings + 0 RSSI when offline.
    if (network_ && network_->isOnline()) {
        const std::string ssid = network_->ssid();
        const std::string ip   = network_->ipAddress();
        const int         dbm  = network_->rssi();
        lv_label_set_text(ssidLbl_, ssid.empty() ? "(unknown SSID)" : ssid.c_str());
        lv_label_set_text(ipLbl_,   ip.empty()   ? "(no ip)"        : ip.c_str());

        // Bar string with per-bar recoloring. lv_label recolor markup:
        //   "#RRGGBB text#"  applies that color to "text" until the
        // closing #. We render 4 fixed bars; first `bars` get accent,
        // the rest get the dim theme colour.
        const int bars = rssiToBars(dbm);
        char buf[80];
        const uint32_t hi = kTheme.accent;
        const uint32_t lo = kTheme.line;
        snprintf(buf, sizeof(buf),
                 "#%06lx %s%s#  #%06lx %s%s#  %d dBm",
                 static_cast<unsigned long>(hi),
                 bars >= 1 ? "|" : " ",
                 bars >= 2 ? "|" : " ",
                 static_cast<unsigned long>(lo),
                 bars >= 3 ? "|" : " ",
                 bars >= 4 ? "|" : " ",
                 dbm);
        lv_label_set_text(rssiLbl_, buf);
    } else {
        lv_label_set_text(ssidLbl_, "(offline)");
        lv_label_set_text(ipLbl_,   "");
        lv_label_set_text(rssiLbl_, "");
    }
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WifiResetView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WifiResetView::render(const feedme::ports::DisplayFrame&) {
    // Snapshot is taken in onEnter(); per-frame refresh would flicker
    // numbers if RSSI fluctuates. Re-entry refreshes.
}

const char* WifiResetView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::Tap:
        case E::Press:
            Serial.println("[wifi] switch confirmed — invoking callback");
            if (onConfirm_) onConfirm_();
            // The callback starts the in-place captive portal (AP+STA).
            // Hand the user off to the status view so they see the AP
            // info + connection state. On simulator the callback is a
            // noop; the WifiSwitchView shows "(no portal)" and
            // long-press bounces back.
            return "wifiSwitch";
        case E::RotateCW:
        case E::RotateCCW:
            return "settings";  // cancel
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
