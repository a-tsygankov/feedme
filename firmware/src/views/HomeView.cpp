#include "views/HomeView.h"

#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {

// Layout constants mirror SettingsView so the two screens look like
// siblings (same selection arc, same row spacing, same fade math).
constexpr int ROW_HEIGHT_PX  = 32;
constexpr int ROW_SPACING_PX = 38;
constexpr int ICON_DIAM_PX   = 28;
constexpr int LIST_LEFT_PAD  = 30;
constexpr int LIST_RIGHT_PAD = 30;

constexpr int ARC_RADIUS_PX  = 110;
constexpr int ARC_DIAM_PX    = ARC_RADIUS_PX * 2;
constexpr int ARC_ROTATION   = 20;
constexpr int ARC_SWEEP      = 140;

struct ItemSpec {
    const char* label;
    const char* icon;          // LVGL symbol or single-char fallback
    const char* destination;   // ScreenManager view name
};

const ItemSpec kItems[HomeView::ITEM_COUNT] = {
    { "Cats",     "*",                "catsList"          },
    { "Users",    "U",                "usersList"         },
    { "Sync",     LV_SYMBOL_REFRESH,  "syncing"           },
    // Phase F — Login QR: an already-paired device shows a one-shot
    // QR with a 60-s token; phone scans → /qr-login → exchange → in.
    { "Login QR", "L",                "loginQr"           },
    { "Pair",     "Q",                "pairing"           },
    { "Reset",    LV_SYMBOL_WARNING,  "resetPairConfirm"  },
};

// Indices into kItems[] for the rows that need extra rendering /
// gating logic. Centralised so a future re-order doesn't require
// hunting through the file.
constexpr int kSyncIdx    = 2;
constexpr int kLoginQrIdx = 3;

}  // namespace

void HomeView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    selectionArc_ = lv_arc_create(root_);
    lv_obj_set_size(selectionArc_, ARC_DIAM_PX, ARC_DIAM_PX);
    lv_obj_center(selectionArc_);
    lv_arc_set_rotation(selectionArc_, ARC_ROTATION);
    lv_arc_set_bg_angles(selectionArc_, 0, ARC_SWEEP);
    lv_obj_remove_style(selectionArc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(selectionArc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(selectionArc_, lv_color_hex(kTheme.accent), LV_PART_MAIN);
    lv_obj_set_style_arc_width(selectionArc_, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(selectionArc_, LV_OPA_TRANSP, LV_PART_INDICATOR);

    for (int i = 0; i < ITEM_COUNT; ++i) {
        rowContainers_[i] = lv_obj_create(root_);
        lv_obj_set_size(rowContainers_[i],
                        240 - LIST_LEFT_PAD - LIST_RIGHT_PAD, ROW_HEIGHT_PX);
        lv_obj_set_style_bg_opa(rowContainers_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(rowContainers_[i], 0, 0);
        lv_obj_set_style_pad_all(rowContainers_[i], 0, 0);
        lv_obj_set_style_radius(rowContainers_[i], 0, 0);
        lv_obj_clear_flag(rowContainers_[i],
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        rowIcons_[i] = lv_obj_create(rowContainers_[i]);
        lv_obj_set_size(rowIcons_[i], ICON_DIAM_PX, ICON_DIAM_PX);
        lv_obj_set_style_radius(rowIcons_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(rowIcons_[i], 2, 0);
        lv_obj_set_style_bg_opa(rowIcons_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_pad_all(rowIcons_[i], 0, 0);
        lv_obj_clear_flag(rowIcons_[i],
                          LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(rowIcons_[i], LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* iconLbl = lv_label_create(rowIcons_[i]);
        lv_obj_set_style_text_font(iconLbl, &lv_font_montserrat_14, 0);
        lv_label_set_text(iconLbl, kItems[i].icon);
        lv_obj_center(iconLbl);

        rowLabels_[i] = lv_label_create(rowContainers_[i]);
        lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(rowLabels_[i], kItems[i].label);
        lv_obj_align(rowLabels_[i], LV_ALIGN_LEFT_MID, ICON_DIAM_PX + 12, 0);

        rowValues_[i] = lv_label_create(rowContainers_[i]);
        lv_obj_set_style_text_font(rowValues_[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(rowValues_[i], lv_color_hex(kTheme.dim), 0);
        lv_label_set_text(rowValues_[i], "");
        applyClippedLabel(rowValues_[i], 95, LV_TEXT_ALIGN_RIGHT);
        lv_obj_align(rowValues_[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    addBackHint(root_);   // ◀ hold = back to menu
}

void HomeView::redraw() {
    const int catCount = roster_     ? roster_->count()     : -1;
    const int usrCount = userRoster_ ? userRoster_->count() : -1;

    const bool changed = firstRender_
                         || selectedIdx_ != lastDrawnIdx_
                         || catCount     != lastDrawnCatCount_
                         || usrCount     != lastDrawnUsrCount_;
    if (!changed) return;

    for (int i = 0; i < ITEM_COUNT; ++i) {
        const int offset = i - selectedIdx_;
        const bool isSel = (offset == 0);

        lv_obj_align(rowContainers_[i], LV_ALIGN_CENTER, 0, offset * ROW_SPACING_PX);

        lv_opa_t opa;
        const int absOff = offset < 0 ? -offset : offset;
        switch (absOff) {
            case 0:  opa = LV_OPA_COVER; break;
            case 1:  opa = LV_OPA_60;    break;
            default: opa = LV_OPA_TRANSP; break;
        }
        lv_obj_set_style_opa(rowContainers_[i], opa, 0);

        lv_obj_set_style_border_color(rowIcons_[i],
            lv_color_hex(isSel ? kTheme.accent : kTheme.line), 0);
        lv_obj_set_style_text_color(rowLabels_[i],
            lv_color_hex(isSel ? kTheme.ink : kTheme.dim), 0);

        // Per-row dynamic value:
        //   Cats / Users  → live count
        //   Sync          → "—" when unpaired, blank otherwise
        //   Pair / Reset  → no value (action is the destination)
        switch (i) {
            case 0: {
                if (catCount >= 0) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d", catCount);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case 1: {
                if (usrCount >= 0) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d", usrCount);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case kSyncIdx:
            case kLoginQrIdx: {
                // Both Sync and Login QR are paired-only: they hit
                // DeviceToken-gated endpoints and would just 401
                // before pairing completes. Same greying treatment so
                // the user learns the affordance exists but knows it's
                // unavailable until pairing.
                const bool paired = isPaired_ && *isPaired_;
                lv_label_set_text(rowValues_[i], paired ? "" : "pair first");
                if (!paired) {
                    lv_obj_set_style_text_color(rowLabels_[i],
                        lv_color_hex(kTheme.faint), 0);
                }
                break;
            }
            default:
                lv_label_set_text(rowValues_[i], "");
                break;
        }
    }

    lastDrawnIdx_      = selectedIdx_;
    lastDrawnCatCount_ = catCount;
    lastDrawnUsrCount_ = usrCount;
    firstRender_       = false;
}

void HomeView::onEnter() {
    firstRender_  = true;
    lastDrawnIdx_ = -1;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void HomeView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void HomeView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* HomeView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::RotateCW:
            selectedIdx_ = (selectedIdx_ + 1) % ITEM_COUNT;
            return nullptr;
        case E::RotateCCW:
            selectedIdx_ = (selectedIdx_ + ITEM_COUNT - 1) % ITEM_COUNT;
            return nullptr;
        case E::Tap:
        case E::Press: {
            // Sync + Login-QR rows are gated by the "is paired?"
            // pointer. Tapping either before pairing would just fail
            // with 401 "device token required" — refuse the transition
            // and give the user a hint via the greyed "pair first"
            // value text.
            if ((selectedIdx_ == kSyncIdx || selectedIdx_ == kLoginQrIdx)
                && (!isPaired_ || !*isPaired_)) {
                Serial.printf("[home] tap on %s ignored (not paired)\n",
                              kItems[selectedIdx_].label);
                return nullptr;
            }
            // Loud serial logging because previous "Pair didn't react"
            // bug reports were hard to triage without it. If you ever
            // see "[home] tap idx=N" but no follow-up screen change,
            // ScreenManager::transition rejected the destination
            // (look for "[screen] unknown view 'X'" right after).
            const char* dest = kItems[selectedIdx_].destination;
            Serial.printf("[home] tap/press idx=%d -> %s\n",
                          selectedIdx_, dest ? dest : "(null)");
            return dest;
        }
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
