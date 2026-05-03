#include "views/PairingProgressView.h"

#include "application/SyncService.h"
#include "ports/IPreferences.h"
#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

void PairingProgressView::build(lv_obj_t* parent) {
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
    lv_obj_set_style_text_color(titleLbl_, lv_color_hex(kTheme.ink), 0);
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(titleLbl_, "Pairing");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, -20);

    dotsLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(dotsLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(dotsLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(dotsLbl_, "...");
    lv_obj_align(dotsLbl_, LV_ALIGN_CENTER, 0, 14);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_,
        "Confirm in the web app.\nLong-press to cancel.");
    lv_obj_set_style_text_align(hintLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hintLbl_, LV_ALIGN_CENTER, 0, 56);
}

void PairingProgressView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    enteredMs_  = millis();
    lastPollMs_ = 0;     // poll immediately on first render tick
    lastDotsMs_ = millis();
    dotsFrame_  = 0;
    terminal_   = nullptr;
    startedOk_  = false;

    // Open the 3-minute window server-side. If this fails (network,
    // bad deviceId), the next render bails with NetworkError → idle.
    if (svc_) {
        svc_->clearCancel();
        startedOk_ = svc_->pairStart();
        if (!startedOk_) {
            Serial.println("[pairProg] pairStart failed; will exit on next tick");
            terminal_ = "idle";
        }
    }
    lv_label_set_text(titleLbl_, "Pairing");
    lv_label_set_text(dotsLbl_, ".");
}

void PairingProgressView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void PairingProgressView::render(const feedme::ports::DisplayFrame&) {
    if (terminal_) return;        // already decided to leave; ScreenManager polls nextView
    if (!svc_)     return;

    const uint32_t now = millis();

    // 3-minute timeout — server-side window will have expired anyway.
    if (now - enteredMs_ >= TIMEOUT_MS) {
        Serial.println("[pairProg] 3-min timeout — back to idle");
        terminal_ = "idle";
        return;
    }

    // Animate the dots: rotate "." → ".." → "..." → ".".
    if (now - lastDotsMs_ >= DOTS_INTERVAL_MS) {
        lastDotsMs_ = now;
        dotsFrame_  = (dotsFrame_ + 1) % 3;
        const char* frames[] = {".", "..", "..."};
        lv_label_set_text(dotsLbl_, frames[dotsFrame_]);
    }

    // Poll /api/pair/check at the configured interval. The first
    // poll fires immediately (lastPollMs_=0) so a fast confirmation
    // lands in <500 ms instead of waiting the full 15 s.
    if (now - lastPollMs_ >= POLL_INTERVAL_MS || lastPollMs_ == 0) {
        lastPollMs_ = now;
        using PR = feedme::application::SyncService::PairResult;
        const PR result = svc_->pairCheck();
        switch (result) {
            case PR::Pending:
                /* keep dots spinning */
                break;
            case PR::Confirmed:
                Serial.println("[pairProg] confirmed — persisting token + home name");
                if (prefs_) {
                    prefs_->setDeviceToken(svc_->deviceToken().c_str());
                    prefs_->setHomeName   (svc_->homeName().c_str());
                    prefs_->setPaired(true);
                }
                // Hand off to SyncingView for the initial roster sync.
                terminal_ = "syncing";
                break;
            case PR::Expired:
            case PR::Cancelled:
            case PR::NetworkError:
            case PR::UnknownStatus:
                Serial.printf("[pairProg] terminal pair result=%d → idle\n",
                              static_cast<int>(result));
                terminal_ = "idle";
                break;
        }
    }
}

const char* PairingProgressView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::LongPress:
        case E::LongTouch:
            // User abort. Tell the server the pairing is dead so the
            // webapp's polling sees "cancelled" and shows an error
            // banner instead of a token.
            if (svc_) {
                svc_->requestCancel();
                svc_->pairCancel();
            }
            lv_label_set_text(titleLbl_, "Cancelled");
            lv_label_set_text(dotsLbl_, "");
            terminal_ = "idle";
            return nullptr;
        default:
            // Single tap / rotate are no-ops here — the user's job is
            // to confirm in the webapp, not interact with the device.
            return nullptr;
    }
}

const char* PairingProgressView::nextView() {
    return terminal_;
}

}  // namespace feedme::views
