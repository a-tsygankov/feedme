#include "views/PairingView.h"

#include "views/Theme.h"

#include <Arduino.h>
#include <string.h>

// LVGL ships its own QR generator (extra/libs/qrcode). Enabled via
// LV_USE_QRCODE=1 in include/lv_conf.h.
#include <src/extra/libs/qrcode/lv_qrcode.h>

namespace feedme::views {

namespace {

// QR module size + quiet zone tuned for 240×240 round display:
//   - URL is ~64 chars (https://feedme-webapp.pages.dev/setup?hid=feedme-abcdef)
//   - That fits in QR version 4 ECC_LOW = 33×33 modules
//   - 4 px per module + small margin → ~140 px QR centered on screen
//   - Leaves room for a 22px title above and a 14px hid line below
constexpr lv_coord_t kQrSize = 140;

}  // namespace

void PairingView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    titleLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(titleLbl_, "Scan to pair");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 22);

    // QR canvas — created with a placeholder URL; updated in onEnter
    // each time the screen is shown (the hid may change after a reset).
    // Dark = ink, light = white for max contrast on the round display.
    qrcode_ = lv_qrcode_create(root_,
                               kQrSize,
                               lv_color_hex(kTheme.ink),
                               lv_color_white());
    lv_obj_align(qrcode_, LV_ALIGN_CENTER, 0, -6);
    // White margin around the modules — required for camera detection.
    lv_obj_set_style_border_color(qrcode_, lv_color_white(), 0);
    lv_obj_set_style_border_width(qrcode_, 4, 0);

    hidLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hidLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(hidLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hidLbl_, "");
    lv_obj_align(hidLbl_, LV_ALIGN_BOTTOM_MID, 0, -38);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "tap to skip");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void PairingView::onEnter() {
    // Re-encode the QR each time we enter — the hid may have rotated
    // (Reset pairing path) since the last time this view was visible.
    if (qrcode_ && url_ && url_[0]) {
        lv_qrcode_update(qrcode_, url_, strlen(url_));
    }
    if (hidLbl_) {
        lv_label_set_text(hidLbl_, hid_);
    }
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    Serial.printf("[pairing] showing hid='%s' url='%s'\n", hid_, url_);
}

void PairingView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairingView::render(const feedme::ports::DisplayFrame&) {
    // Static — nothing to refresh per frame.
}

const char* PairingView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::Tap:
        case E::Press:
            // User dismissed — mark paired so we don't re-show on
            // subsequent boots, then fall back to the home screen.
            if (onPaired_) onPaired_();
            return "idle";
        case E::LongPress:
        case E::LongTouch:
            // Deliberate long-press is the "I forgot the PIN / starting
            // over" gesture. Routes to the confirm screen which rotates
            // the hid and reboots after a tap. Cancel from there
            // (long-press) bounces back here via parent() = "pairing".
            return "resetPairConfirm";
        default:
            // Rotation is a no-op on the pairing screen.
            return nullptr;
    }
}

}  // namespace feedme::views
