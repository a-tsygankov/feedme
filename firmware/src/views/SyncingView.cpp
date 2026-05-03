#include "views/SyncingView.h"

#include "application/SyncService.h"
#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

void SyncingView::build(lv_obj_t* parent) {
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
    lv_label_set_text(titleLbl_, "Syncing");
    lv_obj_align(titleLbl_, LV_ALIGN_CENTER, 0, -20);

    dotsLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(dotsLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(dotsLbl_, &lv_font_montserrat_24, 0);
    lv_label_set_text(dotsLbl_, ".");
    lv_obj_align(dotsLbl_, LV_ALIGN_CENTER, 0, 14);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "Long-press to cancel");
    lv_obj_align(hintLbl_, LV_ALIGN_CENTER, 0, 56);
}

void SyncingView::onEnter() {
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
    enteredMs_  = millis();
    lastDotsMs_ = enteredMs_;
    dotsFrame_  = 0;
    finishedMs_ = 0;
    phase_      = Phase::Starting;
    lv_label_set_text(titleLbl_, "Syncing");
    lv_label_set_text(dotsLbl_, ".");
    lv_label_set_text(hintLbl_, "Long-press to cancel");
    if (svc_) svc_->clearCancel();
}

void SyncingView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SyncingView::render(const feedme::ports::DisplayFrame&) {
    if (!svc_) return;
    const uint32_t now = millis();

    // Dots animation across all phases. We bump even after the call
    // returns (Done/Failed) so the brief result-hold window doesn't
    // look frozen — the user reads the result text against a still
    // animating tail.
    if (now - lastDotsMs_ >= DOTS_INTERVAL_MS) {
        lastDotsMs_ = now;
        dotsFrame_  = (dotsFrame_ + 1) % 3;
        const char* frames[] = {".", "..", "..."};
        lv_label_set_text(dotsLbl_, frames[dotsFrame_]);
    }

    // Kick off sync exactly once, on first render after entry.
    // Doing it here (not in onEnter) gives LVGL a frame to paint
    // the splash before the blocking HTTP call, so the user sees
    // "Syncing ..." rather than a frozen idle for the round-trip.
    if (phase_ == Phase::Starting) {
        phase_ = Phase::Working;
        Serial.println("[syncing] starting POST /api/sync");
        const bool ok = svc_->syncFull();
        finishedMs_ = millis();
        if (svc_->cancelRequested()) {
            phase_ = Phase::Cancelled;
            lv_label_set_text(titleLbl_, "Cancelled");
            lv_label_set_text(hintLbl_, "");
        } else if (ok) {
            phase_ = Phase::Done;
            lv_label_set_text(titleLbl_, "Synced");
            lv_label_set_text(hintLbl_, "");
        } else {
            phase_ = Phase::Failed;
            lv_label_set_text(titleLbl_, "Failed");
            lv_label_set_text(hintLbl_, svc_->lastError().c_str());
        }
    }
}

const char* SyncingView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    if (ev == E::LongPress || ev == E::LongTouch) {
        if (svc_) svc_->requestCancel();
        // The flag is checked at the post-response gate inside
        // SyncService::syncFull. Worst case the user waits the
        // 5-s HTTP timeout before the screen actually leaves.
        return nullptr;
    }
    return nullptr;
}

const char* SyncingView::nextView() {
    if (phase_ == Phase::Starting || phase_ == Phase::Working) return nullptr;
    // Result-hold: keep the splash visible for a beat so the user
    // reads "Synced" / "Failed" / "Cancelled" before we transition.
    if (millis() - finishedMs_ < RESULT_HOLD_MS) return nullptr;
    return "idle";
}

}  // namespace feedme::views
