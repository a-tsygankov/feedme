#include "views/WifiSwitchView.h"

#include "views/Theme.h"

#include <Arduino.h>

namespace feedme::views {

namespace {
constexpr uint32_t DONE_HOLD_MS = 1500;  // brief "Connected" splash before bounce
}

void WifiSwitchView::build(lv_obj_t* parent) {
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
    lv_obj_set_style_text_font(titleLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(titleLbl_, "Switch Wi-Fi");
    lv_obj_align(titleLbl_, LV_ALIGN_TOP_MID, 0, 36);

    line1Lbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(line1Lbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(line1Lbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(line1Lbl_, "");
    lv_obj_align(line1Lbl_, LV_ALIGN_TOP_MID, 0, 78);

    primaryLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(primaryLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(primaryLbl_, &lv_font_montserrat_18, 0);
    lv_label_set_text(primaryLbl_, "");
    // Cap to safe chord width for top region; long SSIDs truncate.
    lv_obj_set_width(primaryLbl_, 200);
    lv_label_set_long_mode(primaryLbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(primaryLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(primaryLbl_, LV_ALIGN_TOP_MID, 0, 98);

    line3Lbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(line3Lbl_, lv_color_hex(kTheme.dim), 0);
    lv_obj_set_style_text_font(line3Lbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(line3Lbl_, "");
    lv_obj_align(line3Lbl_, LV_ALIGN_TOP_MID, 0, 138);

    secondaryLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(secondaryLbl_, lv_color_hex(kTheme.accent), 0);
    lv_obj_set_style_text_font(secondaryLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(secondaryLbl_, "");
    lv_obj_set_width(secondaryLbl_, 200);
    lv_label_set_long_mode(secondaryLbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(secondaryLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(secondaryLbl_, LV_ALIGN_TOP_MID, 0, 158);

    hintLbl_ = lv_label_create(root_);
    lv_obj_set_style_text_color(hintLbl_, lv_color_hex(kTheme.faint), 0);
    lv_obj_set_style_text_font(hintLbl_, &lv_font_montserrat_14, 0);
    lv_label_set_text(hintLbl_, "HOLD  CANCEL");
    lv_obj_set_width(hintLbl_, 140);
    lv_label_set_long_mode(hintLbl_, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(hintLbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hintLbl_, LV_ALIGN_BOTTOM_MID, 0, -22);
}

void WifiSwitchView::onEnter() {
    lastDrawnState_ = -1;  // force a render
    doneEnteredMs_  = 0;
    pendingExit_    = false;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void WifiSwitchView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    // Tear down the portal so the AP MAC is dropped and the device is
    // left on whatever STA became (new network on success, old network
    // on cancel/failure).
    if (portal_) portal_->stop();
}

void WifiSwitchView::render(const feedme::ports::DisplayFrame&) {
    if (!portal_) return;
    using S = feedme::adapters::WifiCaptivePortal::State;
    const S s = portal_->state();
    const int sCast = static_cast<int>(s);
    if (sCast == lastDrawnState_) {
        // Still note the moment we first hit Done so the splash holds
        // for DONE_HOLD_MS before nextView() bounces back to settings.
        if (s == S::Done && doneEnteredMs_ == 0) doneEnteredMs_ = millis();
        return;
    }
    lastDrawnState_ = sCast;

    switch (s) {
        case S::Advertising:
            lv_label_set_text(line1Lbl_,     "connect to");
            lv_label_set_text(primaryLbl_,   portal_->apName());
            lv_obj_set_style_text_color(primaryLbl_,
                lv_color_hex(kTheme.accent), 0);
            lv_label_set_text(line3Lbl_,     "then open");
            lv_label_set_text(secondaryLbl_, portal_->apIp());
            lv_obj_set_style_text_color(secondaryLbl_,
                lv_color_hex(kTheme.accent), 0);
            break;
        case S::Switching:
            lv_label_set_text(line1Lbl_,     "joining");
            lv_label_set_text(primaryLbl_,   portal_->targetSsid());
            lv_obj_set_style_text_color(primaryLbl_,
                lv_color_hex(kTheme.accent), 0);
            lv_label_set_text(line3Lbl_,     "");
            lv_label_set_text(secondaryLbl_, "...");
            break;
        case S::Done:
            lv_label_set_text(line1Lbl_,     "connected to");
            lv_label_set_text(primaryLbl_,   portal_->targetSsid());
            lv_obj_set_style_text_color(primaryLbl_,
                lv_color_hex(kTheme.accent), 0);
            lv_label_set_text(line3Lbl_,     "");
            lv_label_set_text(secondaryLbl_, "");
            doneEnteredMs_ = millis();
            break;
        case S::Failed:
            lv_label_set_text(line1Lbl_,     "couldn't join");
            lv_label_set_text(primaryLbl_,   portal_->targetSsid());
            lv_obj_set_style_text_color(primaryLbl_,
                lv_color_hex(kTheme.dim), 0);
            lv_label_set_text(line3Lbl_,     "still on AP");
            lv_label_set_text(secondaryLbl_, "retry from phone");
            lv_obj_set_style_text_color(secondaryLbl_,
                lv_color_hex(kTheme.dim), 0);
            break;
        case S::Idle:
            lv_label_set_text(line1Lbl_,     "");
            lv_label_set_text(primaryLbl_,   "(no portal)");
            lv_label_set_text(line3Lbl_,     "");
            lv_label_set_text(secondaryLbl_, "");
            break;
    }
}

const char* WifiSwitchView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    // Tap / Press while in Done = "ok, back to settings now". Otherwise
    // input is a no-op (interaction is on the phone). Long-press =
    // ScreenManager fallback to parent (cancel back to settings;
    // onLeave stops the portal).
    if (ev == E::Tap || ev == E::Press) {
        if (portal_ && portal_->state() ==
                feedme::adapters::WifiCaptivePortal::State::Done) {
            return "settings";
        }
    }
    return nullptr;
}

const char* WifiSwitchView::nextView() {
    if (!portal_) return nullptr;
    using S = feedme::adapters::WifiCaptivePortal::State;
    if (portal_->state() == S::Done && doneEnteredMs_ != 0
        && millis() - doneEnteredMs_ >= DONE_HOLD_MS) {
        return "settings";
    }
    return nullptr;
}

}  // namespace feedme::views
