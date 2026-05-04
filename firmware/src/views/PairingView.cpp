#include "views/PairingView.h"

#include "application/SyncService.h"
#include "ports/IPreferences.h"
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

    // QR canvas. MUST be pure black-on-white — phone cameras lose the
    // pattern under any other contrast. Don't reach for kTheme.ink
    // here: on the dark Aubergine theme that's a near-white cream
    // (designed for text on bg), and pairing it with a white canvas
    // background paints invisible modules.
    qrcode_ = lv_qrcode_create(root_,
                               kQrSize,
                               lv_color_black(),
                               lv_color_white());
    lv_obj_align(qrcode_, LV_ALIGN_CENTER, 0, -6);
    // White margin around the modules — the QR spec's "quiet zone"
    // (4-module wide blank border). Without it some decoders refuse
    // to lock onto the finder patterns.
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
    lv_label_set_text(hintLbl_, "long-press for Reset");
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

    // Open the 3-minute pair window server-side IMMEDIATELY. Without
    // this the user could scan the QR + sign in on the webapp before
    // /pair/start ever fires, and the backend's confirmPairingFor
    // would 404 (no pending row), leaving the device stuck on this
    // screen forever. lastPollMs_=0 makes the first /pair/check fire
    // on the very next render() tick so a fast confirmation lands
    // sub-second instead of waiting 15 s.
    enteredMs_  = millis();
    lastPollMs_ = 0;
    terminal_   = nullptr;
    startedOk_  = false;
    if (svc_) {
        svc_->clearCancel();
        startedOk_ = svc_->pairStart();
        if (!startedOk_) {
            // Network failed; show the QR anyway but the auto-pair
            // poll won't have anything to find. Render() leaves the
            // view active so a long-press → Reset is still reachable.
            Serial.println("[pairing] pairStart failed; QR shown but no auto-pair poll");
        }
    }
}

void PairingView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairingView::render(const feedme::ports::DisplayFrame&) {
    if (terminal_) return;        // already decided to leave; ScreenManager polls nextView()
    if (!svc_)     return;
    if (!startedOk_) return;      // no pending_pairings row to poll for

    const uint32_t now = millis();
    if (now - lastPollMs_ < POLL_INTERVAL_MS && lastPollMs_ != 0) return;
    lastPollMs_ = now;

    using PR = feedme::application::SyncService::PairResult;
    const PR result = svc_->pairCheck();
    switch (result) {
        case PR::Pending:
            /* keep waiting; QR stays visible */
            break;
        case PR::Confirmed:
            Serial.println("[pairing] confirmed — persisting token + home name");
            if (prefs_) {
                prefs_->setDeviceToken(svc_->deviceToken().c_str());
                prefs_->setHomeName   (svc_->homeName().c_str());
                prefs_->setPaired(true);
            }
            // Back-compat: fire the onPaired callback so any caller
            // listening for "user successfully paired" still gets
            // notified (main.cpp uses this to flip a runtime flag).
            if (onPaired_) onPaired_();
            // Hand off to the SyncingView for the initial roster sync.
            terminal_ = "syncing";
            break;
        case PR::Expired:
        case PR::Cancelled:
            // Server window timed out OR was cancelled by /pair/cancel.
            // Reopen by simply re-entering this view. We restart from
            // scratch via onEnter — pair/start is INSERT OR REPLACE so
            // a re-call is safe.
            Serial.printf("[pairing] window terminal (%d) — restarting\n",
                          static_cast<int>(result));
            startedOk_ = svc_ ? svc_->pairStart() : false;
            lastPollMs_ = millis();   // back off one full interval
            break;
        case PR::NetworkError:
        case PR::UnknownStatus:
            // Transient. Keep the QR up and try the next poll cycle.
            // Don't churn pair/start on every poll — that would burn
            // through pending_pairings rows uselessly.
            Serial.printf("[pairing] poll error (%d) — retrying next cycle\n",
                          static_cast<int>(result));
            break;
    }
}

const char* PairingView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::LongPress:
        case E::LongTouch:
            // Deliberate long-press is the "I forgot the PIN / starting
            // over" gesture. Routes to the confirm screen which deletes
            // the server-side pairing, wipes local NVS, regenerates the
            // device id, and reboots. Cancel from there (long-press)
            // bounces back here via parent() = "pairing".
            return "resetPairConfirm";
        case E::Tap:
        case E::Press:
            // Tap is now a no-op — the user's only job is to scan the
            // QR with their phone and sign in via the webapp; the
            // poll loop in render() does the rest. We deliberately
            // DON'T transition to PairingProgressView anymore: PR #34
            // made the auth handlers do pair-confirm inline, so the
            // separate "tap to begin pairing" step is gone. The
            // hint label points at long-press → Reset as the only
            // remaining device-side affordance.
            return nullptr;
        default:
            return nullptr;
    }
}

const char* PairingView::nextView() {
    return terminal_;
}

}  // namespace feedme::views
