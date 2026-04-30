#include "views/SettingsView.h"

#include "application/DisplayCoordinator.h"
#include "views/LabelHelpers.h"
#include "views/Theme.h"

#include <Arduino.h>
#include <stdio.h>

namespace feedme::views {

namespace {

constexpr int ROW_HEIGHT_PX  = 32;
constexpr int ROW_SPACING_PX = 38;   // matches ScrSettings.y = 120 + offset*38
constexpr int ICON_DIAM_PX   = 28;
constexpr int LIST_LEFT_PAD  = 30;
constexpr int LIST_RIGHT_PAD = 30;

constexpr int ARC_RADIUS_PX  = 110;
constexpr int ARC_DIAM_PX    = ARC_RADIUS_PX * 2;
// Selection arc on the left perimeter: design path -> arcPath(120,120,110,200,340).
// 200°→340° in design coords (0° at 3 o'clock, CCW positive). Translating to
// LVGL (0° at 3 o'clock, CW positive): start = 360 - 340 = 20, end = 360 - 200 = 160.
// Equivalently: rotate by 20°, sweep 0..140.
constexpr int ARC_ROTATION   = 20;
constexpr int ARC_SWEEP      = 140;

struct ItemSpec {
    const char* label;
    const char* icon;   // LVGL symbol or single-char fallback
};

// Order must match the indices used in handleInput() and redraw().
//
// Quiet hours used to live at index 2; removed because the same toggle
// is reachable from the main F-S-Q-G menu (the "Q" glyph), so the
// Settings entry was duplicative. The slot is now a placeholder for
// Notifications (Phase 4.1) — value renders as "—" and tap is a
// no-op until the editor lands.
const ItemSpec kItems[SettingsView::ITEM_COUNT] = {
    { "Wi-Fi",     LV_SYMBOL_WIFI },
    { "Wake",      "K"            },   // no built-in sun glyph
    { "Notify",    LV_SYMBOL_BELL },   // placeholder — Phase 4.1 push notifications
    { "Threshold", LV_SYMBOL_SETTINGS },
    { "Cats",      "*"            },   // no paw glyph; placeholder for IcCat
    { "Users",     "U"            },   // placeholder for IcUser
    { "Timezone",  "Z"            },   // placeholder for IcGlobe
    { "Sleep",     LV_SYMBOL_POWER }, // power-symbol stand-in for sleep timeout
};

}  // namespace

void SettingsView::build(lv_obj_t* parent) {
    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, 240, 240);
    lv_obj_center(root_);
    lv_obj_set_style_bg_color(root_, lv_color_hex(kTheme.bg), 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Selection arc on the left edge.
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

    // Four rows, all anchored at centre with a per-row Y offset applied
    // in redraw(). Positioning is relative to the *selected* row, so
    // changing selectedIdx_ slides every row — the same pattern as the
    // JSX where each row's y = 120 + (i - idx) * 38.
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

        // Icon disc on the left.
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

        // Row label (Wi-Fi / Wake / ...).
        rowLabels_[i] = lv_label_create(rowContainers_[i]);
        lv_obj_set_style_text_font(rowLabels_[i], &lv_font_montserrat_14, 0);
        lv_label_set_text(rowLabels_[i], kItems[i].label);
        lv_obj_align(rowLabels_[i], LV_ALIGN_LEFT_MID, ICON_DIAM_PX + 12, 0);

        // Right-aligned value label. Cap to ~95 px and truncate with
        // an ellipsis so long values (mostly long SSIDs on the Wi-Fi
        // row) don't overrun the icon/label on the left.
        rowValues_[i] = lv_label_create(rowContainers_[i]);
        lv_obj_set_style_text_font(rowValues_[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(rowValues_[i], lv_color_hex(kTheme.dim), 0);
        lv_label_set_text(rowValues_[i], "");
        applyClippedLabel(rowValues_[i], 95, LV_TEXT_ALIGN_RIGHT);
        lv_obj_align(rowValues_[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

void SettingsView::redraw() {
    const bool online    = network_ ? network_->isOnline() : false;
    const std::string ssid = network_ ? network_->ssid() : std::string();
    const bool quietOn   = quiet_   ? quiet_->enabled()    : false;
    const int  wakeH     = wake_    ? wake_->hour()        : -1;
    const int  wakeM     = wake_    ? wake_->minute()      : -1;
    const long thresholdSec = coord_ ? static_cast<long>(coord_->hungryThresholdSec())
                                     : -1L;
    const int  catCount     = roster_      ? roster_->count()      : -1;
    const int  userCount    = userRoster_  ? userRoster_->count()  : -1;
    const int  tzMin        = tz_          ? tz_->offsetMin()      : -99999;
    const int  sleepMin     = sleep_       ? sleep_->minutes()     : -99999;

    const bool changed = firstRender_
                         || selectedIdx_ != lastDrawnIdx_
                         || online       != lastDrawnOnline_
                         || ssid         != lastDrawnSsid_
                         || quietOn      != lastDrawnQuietEnabled_
                         || wakeH        != lastDrawnWakeHour_
                         || wakeM        != lastDrawnWakeMinute_
                         || thresholdSec != lastDrawnThresholdSec_
                         || catCount     != lastDrawnCatCount_
                         || userCount    != lastDrawnUserCount_
                         || tzMin        != lastDrawnTzMin_
                         || sleepMin     != lastDrawnSleepMin_;
    if (!changed) return;

    for (int i = 0; i < ITEM_COUNT; ++i) {
        const int offset = i - selectedIdx_;
        const bool isSel = (offset == 0);

        // Vertical position: centred row sits at (0,0) from screen centre,
        // others slide up/down by ROW_SPACING_PX.
        lv_obj_align(rowContainers_[i], LV_ALIGN_CENTER, 0, offset * ROW_SPACING_PX);

        // Opacity fade: 1.0, 0.55, 0.10, 0 (matches JSX 1 - |offset|*0.45).
        lv_opa_t opa;
        const int absOff = offset < 0 ? -offset : offset;
        switch (absOff) {
            case 0:  opa = LV_OPA_COVER; break;
            case 1:  opa = LV_OPA_60;    break;
            default: opa = LV_OPA_TRANSP; break;
        }
        lv_obj_set_style_opa(rowContainers_[i], opa, 0);

        // Icon disc border + label colour shift on selection.
        lv_obj_set_style_border_color(rowIcons_[i],
            lv_color_hex(isSel ? kTheme.accent : kTheme.line), 0);
        lv_obj_set_style_text_color(rowLabels_[i],
            lv_color_hex(isSel ? kTheme.ink : kTheme.dim), 0);

        // Per-row dynamic value:
        //   Wi-Fi      → online / offline
        //   Wake       → HH:MM live from WakeTime (Phase D.1)
        //   Quiet      → on / off (live from QuietWindow)
        //   Calibrate  → "" (no value)
        switch (i) {
            case 0: {
                // Wi-Fi: show actual SSID when associated, "offline"
                // otherwise. Keeps the "what am I on?" answer in front
                // of the user before they decide to switch. Long SSIDs
                // truncate via the LONG_DOT mode set on the value
                // label below.
                if (online && !ssid.empty()) {
                    lv_label_set_text(rowValues_[i], ssid.c_str());
                } else if (online) {
                    lv_label_set_text(rowValues_[i], "online");
                } else {
                    lv_label_set_text(rowValues_[i], "offline");
                }
                break;
            }
            case 1: {
                if (wake_) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%02d:%02d", wakeH, wakeM);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case 2:
                // Notify placeholder — Phase 4.1 push notifications.
                // Em-dash signals "not yet configurable" without
                // shouting placeholder noise like "tbd".
                lv_label_set_text(rowValues_[i], "-");
                break;
            case 3: {
                if (coord_) {
                    const int totalMin = static_cast<int>(thresholdSec / 60);
                    const int hours    = totalMin / 60;
                    const int minutes  = totalMin % 60;
                    char buf[10];
                    if (minutes == 0) snprintf(buf, sizeof(buf), "%dh",      hours);
                    else              snprintf(buf, sizeof(buf), "%dh%02d",  hours, minutes);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "");
                }
                break;
            }
            case 4: {
                if (catCount >= 0) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d", catCount);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case 5: {
                if (userCount >= 0) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%d", userCount);
                    lv_label_set_text(rowValues_[i], buf);
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case 6: {
                if (tz_) {
                    if (tzMin == 0) {
                        lv_label_set_text(rowValues_[i], "UTC");
                    } else {
                        const char sign = tzMin > 0 ? '+' : '-';
                        const int absM = tzMin > 0 ? tzMin : -tzMin;
                        char buf[10];
                        if (absM % 60 == 0) {
                            snprintf(buf, sizeof(buf), "%c%d", sign, absM / 60);
                        } else {
                            snprintf(buf, sizeof(buf), "%c%d:%02d",
                                     sign, absM / 60, absM % 60);
                        }
                        lv_label_set_text(rowValues_[i], buf);
                    }
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
            case 7: {
                // Sleep timeout. 0 minutes → "--" (sleep disabled);
                // matches the editor's display so the user sees the
                // same string both places.
                if (sleep_) {
                    const int m = sleep_->minutes();
                    if (m == 0) {
                        lv_label_set_text(rowValues_[i], "--");
                    } else {
                        char buf[10];
                        snprintf(buf, sizeof(buf), "%dmin", m);
                        lv_label_set_text(rowValues_[i], buf);
                    }
                } else {
                    lv_label_set_text(rowValues_[i], "-");
                }
                break;
            }
        }
    }

    lastDrawnIdx_              = selectedIdx_;
    lastDrawnOnline_           = online;
    lastDrawnSsid_             = ssid;
    lastDrawnQuietEnabled_     = quietOn;
    lastDrawnWakeHour_         = wakeH;
    lastDrawnWakeMinute_       = wakeM;
    lastDrawnThresholdSec_     = thresholdSec;
    lastDrawnCatCount_         = catCount;
    lastDrawnUserCount_        = userCount;
    lastDrawnTzMin_            = tzMin;
    lastDrawnSleepMin_         = sleepMin;
    firstRender_               = false;
}

void SettingsView::onEnter() {
    firstRender_  = true;
    lastDrawnIdx_ = -1;
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SettingsView::onLeave() {
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void SettingsView::render(const feedme::ports::DisplayFrame&) {
    redraw();
}

const char* SettingsView::handleInput(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    switch (ev) {
        case E::RotateCW:
            selectedIdx_ = (selectedIdx_ + 1) % ITEM_COUNT;
            return nullptr;
        case E::RotateCCW:
            selectedIdx_ = (selectedIdx_ + ITEM_COUNT - 1) % ITEM_COUNT;
            return nullptr;
        case E::Tap:
        case E::Press:
            // Phase D dispatch — each row maps to its sub-editor view.
            // Items not yet implemented log the intent and stay put.
            switch (selectedIdx_) {
                case 0:  // Wi-Fi reset confirmation (Phase D.4) — wired
                    return "wifiReset";
                case 1:  // Wake-time editor (Phase D.1) — wired
                    return "wakeTimeEdit";
                case 2:  // Notify placeholder — Phase 4.1; no-op for now
                    return nullptr;
                case 3:  // Threshold editor (Phase D.3) — wired
                    return "thresholdEdit";
                case 4:  // Cats list (Phase D.5) — wired
                    return "catsList";
                case 5:  // Users list (Phase D.6) — wired
                    return "usersList";
                case 6:  // Timezone editor — wired
                    return "timezoneEdit";
                case 7:  // Sleep timeout editor — wired
                    return "sleepTimeoutEdit";
            }
            return nullptr;
        // Long-press / long-touch → ScreenManager fallback to parent().
        default:
            return nullptr;
    }
}

}  // namespace feedme::views
