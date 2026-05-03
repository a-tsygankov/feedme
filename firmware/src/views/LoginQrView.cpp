#include "views/LoginQrView.h"

#include "application/SyncService.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include <src/extra/libs/qrcode/lv_qrcode.h>

namespace feedme::views {

namespace {

// Webapp deep-link host — same as PairingView. If you need to point
// the device at a different deployment (staging / preview), change
// it here AND in PairingView. Lowercase only — phone QR scanners
// often case-fold.
constexpr const char* kWebappHost = "https://feedme-webapp.pages.dev";

// Same QR canvas size as PairingView (140 px) — the URL fits in QR
// version 5 ECC_LOW (37×37 modules at ~3.7 px/module).
constexpr lv_coord_t kQrSize = 140;

}  // namespace

void LoginQrView::build(lv_obj_t* parent) {
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
    lv_label_set_text(titleLbl_, "Scan to log in");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 22);

    // Black-on-white QR canvas — see PairingView for why kTheme.ink
    // would lose modules on the dark theme.
    qrcode_ = lv_qrcode_create(root_,
                               kQrSize,
                               lv_color_black(),
                               lv_color_white());
    lv_obj_align(qrcode_, LV_ALIGN_CENTER, 0, -6);
    lv_obj_set_style_border_color(qrcode_, lv_color_white(), 0);
    lv_obj_set_style_border_width(qrcode_, 4, 0);

    timerLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(timerLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(timerLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(timerLbl_, "");
    lv_obj_align(timerLbl_, LV_ALIGN_BOTTOM_MID, 0, -38);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "long-press to exit");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void LoginQrView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    enteredMs_    = millis();
    failedAtMs_   = 0;
    lastSecsLeft_ = -1;
    phase_        = Phase::Starting;
    payloadBuf_[0] = '\0';
    lv_label_set_text(titleLbl_, "Minting token");
    lv_label_set_text(timerLbl_, "");
    lv_label_set_text(hintLbl_, "");
    // Hide the QR until we have something to encode — looks weird to
    // flash an empty white square at the user.
    lv_obj_add_flag(qrcode_, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[loginQr] entered");
}

void LoginQrView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void LoginQrView::render(const feedme::ports::DisplayFrame&) {
    if (!svc_) return;
    const uint32_t now = millis();

    // Fire the token request exactly once, on first render after entry.
    // Mirrors SyncingView's pattern: gives LVGL a frame to paint
    // "Minting token" before the synchronous HTTP call freezes us.
    if (phase_ == Phase::Starting) {
        const bool ok = svc_->loginTokenCreate();
        if (!ok || svc_->loginToken().empty()) {
            phase_       = Phase::Failed;
            failedAtMs_  = now;
            lv_label_set_text(titleLbl_, "Failed");
            lv_label_set_text(hintLbl_, svc_->lastError().c_str());
            return;
        }
        // Build the deep-link payload. Same scheme as the webapp
        // /qr-login route handler expects: ?device=<id>&token=<32hex>.
        snprintf(payloadBuf_, sizeof(payloadBuf_),
                 "%s/qr-login?device=%s&token=%s",
                 kWebappHost,
                 svc_->deviceId().c_str(),
                 svc_->loginToken().c_str());
        Serial.printf("[loginQr] payload=%s\n", payloadBuf_);
        lv_qrcode_update(qrcode_, payloadBuf_, strlen(payloadBuf_));
        lv_obj_clear_flag(qrcode_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(titleLbl_, "Scan to log in");
        lv_label_set_text(hintLbl_, "long-press to exit");
        phase_ = Phase::Showing;
        // Reset the entered timestamp NOW so the countdown actually
        // tracks "QR-visible time" rather than "view-entered time"
        // (the HTTP round-trip can be a couple seconds).
        enteredMs_ = now;
        return;
    }

    // Countdown timer — once per visible second. We compare against
    // the wall-clock token expiry isn't available here without a real
    // IClock injection, so we use the local 60-s budget which matches
    // the server's QR_TOKEN_TTL_SEC.
    if (phase_ == Phase::Showing) {
        const int secsElapsed = static_cast<int>((now - enteredMs_) / 1000u);
        const int secsLeft    = TOKEN_TTL_SEC - secsElapsed;
        if (secsLeft <= 0) {
            phase_       = Phase::Expired;
            failedAtMs_  = now;
            lv_label_set_text(titleLbl_, "Expired");
            lv_label_set_text(timerLbl_, "");
            lv_label_set_text(hintLbl_, "open Login QR again");
            lv_obj_add_flag(qrcode_, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (secsLeft != lastSecsLeft_) {
            lastSecsLeft_ = secsLeft;
            char buf[16];
            snprintf(buf, sizeof(buf), "%ds left", secsLeft);
            lv_label_set_text(timerLbl_, buf);
        }
    }
}

const char* LoginQrView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::LongPress:
        case E::LongTouch:
            // User dismiss → back to H menu. Token is left to expire
            // server-side; no cancel endpoint to call.
            return "home";
        // Rotation + tap are no-ops on a QR display screen — the only
        // affordances are scan + long-press to exit.
        default:
            return nullptr;
    }
}

const char* LoginQrView::nextView() {
    // After a token mint failure or post-expiry, hold the result
    // splash for a beat then drop the user back at the H menu so they
    // can pick a different action (or open Login QR again to retry).
    if (phase_ == Phase::Failed || phase_ == Phase::Expired) {
        if (millis() - failedAtMs_ >= FAIL_HOLD_MS) return "home";
    }
    return nullptr;
}

}  // namespace feedme::views
