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
    lv_obj_align(hidLbl_, LV_ALIGN_BOTTOM_MID, 0, -52);

    // Live status footer — tells the user what the device is actually
    // doing behind the scenes. Reads "waiting for sign-in" once we've
    // successfully opened a pair window, "no Wi-Fi…" if pair/start is
    // failing, etc. Updated by setStatus().
    statusLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(statusLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(statusLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(statusLbl_, "");
    lv_obj_align(statusLbl_, LV_ALIGN_BOTTOM_MID, 0, -32);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "tap = retry · long-press = Reset");
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void PairingView::setStatus(const char* msg, bool error) {
    if (!statusLbl_) return;
    // No dedicated "alarm" colour in our palette — accentSoft is the
    // muted rose used for warning gradients elsewhere; close enough
    // to communicate "something's not right" without screaming red.
    lv_obj_set_style_text_color(statusLbl_,
        lv_color_hex(error ? kTheme.accentSoft : kTheme.dim), 0);
    lv_label_set_text(statusLbl_, msg);
}

bool PairingView::tryPairStart() {
    if (!svc_) return false;
    startAttempts_++;
    Serial.printf("[pairing] pairStart attempt #%d\n", startAttempts_);
    const bool ok = svc_->pairStart();
    if (ok) {
        phase_ = Phase::Polling;
        lastTryMs_ = millis();
        setStatus("waiting for sign-in…");
        Serial.println("[pairing] pairStart ok — polling for confirmation");
    } else {
        // Common cause: WiFi still connecting in the first second
        // after boot. Keep retrying; render() schedules us again.
        char buf[40];
        snprintf(buf, sizeof(buf), "no network — retry #%d", startAttempts_);
        setStatus(buf, /*error=*/true);
    }
    return ok;
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

    // Reset state for a fresh handshake every time we enter.
    enteredMs_         = millis();
    lastTryMs_         = 0;        // forces a pair/start on the next render tick
    terminal_          = nullptr;
    phase_             = Phase::Starting;
    startAttempts_     = 0;
    titleFlashUntilMs_ = 0;
    if (titleLbl_) lv_label_set_text(titleLbl_, "Scan to pair");
    if (svc_) svc_->clearCancel();
    setStatus("connecting…");
}

void PairingView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairingView::render(const feedme::ports::DisplayFrame&) {
    if (terminal_) return;        // already decided to leave; ScreenManager polls nextView()
    if (!svc_)     return;

    const uint32_t now = millis();

    // Restore the default title once a tap-flash has elapsed. We can't
    // do this in handleInput because it happens off-frame; doing it
    // here means "Retrying" stays visible for at least one full
    // render cycle even if the network completes faster than 800 ms.
    if (titleFlashUntilMs_ != 0 && now >= titleFlashUntilMs_) {
        titleFlashUntilMs_ = 0;
        if (titleLbl_) lv_label_set_text(titleLbl_, "Scan to pair");
    }

    // ── Phase: Starting — keep retrying pair/start on START_RETRY_MS
    //    cadence until the network comes up. Without this, a device
    //    that boots faster than its WiFi associates would set
    //    startedOk_=false in the old code and never recover.
    if (phase_ == Phase::Starting) {
        if (lastTryMs_ != 0 && now - lastTryMs_ < START_RETRY_MS) return;
        lastTryMs_ = now;
        tryPairStart();
        return;
    }

    // ── Phase: Polling — fire pair/check on POLL_INTERVAL_MS or
    //    immediately if a tap forced it.
    const uint32_t interval = (phase_ == Phase::ForcePollNext) ? 0 : POLL_INTERVAL_MS;
    if (now - lastTryMs_ < interval) return;
    lastTryMs_ = now;
    if (phase_ == Phase::ForcePollNext) {
        phase_ = Phase::Polling;
        setStatus("checking now…");
    }

    using PR = feedme::application::SyncService::PairResult;
    const PR result = svc_->pairCheck();
    switch (result) {
        case PR::Pending:
            setStatus("waiting for sign-in…");
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
            setStatus("paired ✓");
            // Hand off to the SyncingView for the initial roster sync.
            terminal_ = "syncing";
            break;
        case PR::Expired:
        case PR::Cancelled:
            // Server window timed out OR was cancelled. Restart the
            // handshake by going back to Starting — the next render
            // tick will call pair/start (INSERT OR REPLACE; safe to
            // re-call). User sees the status flip to "connecting…".
            Serial.printf("[pairing] window terminal (%d) — restarting\n",
                          static_cast<int>(result));
            phase_ = Phase::Starting;
            lastTryMs_ = 0;          // try again on next tick, no wait
            startAttempts_ = 0;
            setStatus("re-opening window…");
            break;
        case PR::NetworkError:
        case PR::UnknownStatus:
            // Transient on our side. Keep the QR up and try next cycle.
            Serial.printf("[pairing] poll error (%d) — retrying next cycle\n",
                          static_cast<int>(result));
            setStatus("network blip — retrying", /*error=*/true);
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
        case E::Press: {
            // Tap = "force a retry now". Useful in two cases:
            //   1. Device booted before WiFi was ready and pair/start
            //      keeps failing — tap forces an immediate retry
            //      instead of waiting out the START_RETRY_MS gap.
            //   2. User just signed in on the webapp and is impatient
            //      for the device to notice — tap forces a /pair/check
            //      poll right now instead of waiting up to 15 s.
            //
            // Loud Serial log so a user with a serial monitor can
            // confirm the tap event is actually reaching this view —
            // earlier reports had "tap not working" and we couldn't
            // tell if the sensor wasn't firing or if the handler was
            // doing nothing visible. Now it logs unambiguously, AND
            // the title flashes to "Retrying" for a beat so the user
            // gets feedback even without a serial monitor.
            Serial.printf("[pairing] tap (%s) → forcing retry (phase=%d)\n",
                          ev == E::Tap ? "touch" : "encoder",
                          static_cast<int>(phase_));
            if (titleLbl_) lv_label_set_text(titleLbl_, "Retrying");
            titleFlashUntilMs_ = millis() + TITLE_FLASH_MS;
            if (phase_ == Phase::Starting) {
                lastTryMs_ = 0;          // next render tick: immediate retry
                setStatus("tap registered — retrying connection…");
            } else {
                phase_ = Phase::ForcePollNext;
                lastTryMs_ = 0;
                setStatus("tap registered — checking now…");
            }
            return nullptr;
        }
        default:
            return nullptr;
    }
}

const char* PairingView::nextView() {
    return terminal_;
}

}  // namespace feedme::views
