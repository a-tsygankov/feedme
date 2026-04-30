#pragma once

#include "adapters/CatFace.h"
#include "domain/CatRoster.h"
#include "domain/MealSchedule.h"
#include "domain/PortionState.h"
#include "domain/QuietWindow.h"
#include "domain/SleepTimeout.h"
#include "domain/TimeZone.h"
#include "domain/UserRoster.h"
#include "domain/WakeTime.h"
#include "ports/IClock.h"
#include "ports/IDisplay.h"
#include "ports/ITapSensor.h"
#include "views/FedView.h"
#include "views/FeederPickerView.h"
#include "views/FeedConfirmView.h"
#include "views/BootView.h"
#include "views/CatEditView.h"
#include "views/CatRemoveView.h"
#include "views/CatsListView.h"
#include "views/IdleView.h"
#include "views/LockConfirmView.h"
#include "views/MenuView.h"
#include "views/PortionAdjustView.h"
#include "views/PouringView.h"
#include "views/QuietHoursEditView.h"
#include "views/QuietView.h"
#include "views/ScheduleEditView.h"
#include "views/ScheduleView.h"
#include "views/ScreenManager.h"
#include "views/SettingsView.h"
#include "views/SetupView.h"
#include "views/SleepTimeoutEditView.h"
#include "views/ThresholdEditView.h"
#include "views/TimeZoneEditView.h"
#include "views/UserRemoveView.h"
#include "views/UsersListView.h"
#include "views/WakeTimeEditView.h"
#include "views/WifiResetView.h"
#include "views/WifiSwitchView.h"
#if defined(FEEDME_HAS_HOPPER)
#  include "views/HopperView.h"
#endif

#include <lvgl.h>
#include <TFT_eSPI.h>

#include <stdint.h>

namespace feedme::adapters {

// One entry rendered by the history overlay.
struct HistoryItem {
    int64_t ts = 0;            // unix seconds
    char    line[24] = {0};    // pre-formatted "5m ago · feed · Andrey" etc.
};

// LVGL + TFT_eSPI implementation of IDisplay.
// Layout: outer arc ring (color = mood), Simon's Cat-style face inside,
// mood label and time label below the cat, three meal dots at the bottom.
// A history overlay (hidden by default) covers the scene with a list
// of recent events when setHistoryVisible(true) is called.
class LvglDisplay : public feedme::ports::IDisplay {
public:
    static constexpr int HISTORY_MAX = 5;

    void begin() override;
    void render(const feedme::ports::DisplayFrame& frame) override;
    void tick() override;

    // Multi-screen state machine. Pass an input event in to the active
    // view; transitions happen automatically. Returns the new view's
    // name (or null if unchanged) so the dispatcher can side-effect.
    const char* handleInput(feedme::ports::TapEvent ev);
    void        transitionTo(const char* viewName);
    const char* currentView() const;

    // History-overlay control (still owned here for now — pulls double-
    // duty as a transient panel over whichever view is active).
    void setHistory(const HistoryItem* items, int count);
    void setHistoryVisible(bool visible);
    bool historyVisible() const { return historyVisible_; }

    // Accessors so main.cpp can inject FeedingService into PouringView,
    // INetwork into SettingsView, and the sensors + return-to into
    // LockConfirmView; plus read/write the shared portion / quiet state.
    feedme::views::PouringView&     pouringView()  { return pouringView_; }
    feedme::views::SettingsView&    settingsView() { return settingsView_; }
    feedme::views::SetupView&       setupView()    { return setupView_; }
    feedme::views::LockConfirmView& lockConfirmView() { return lockConfirmView_; }
    feedme::views::ThresholdEditView& thresholdEditView() { return thresholdEditView_; }
    feedme::views::WifiResetView&     wifiResetView()     { return wifiResetView_; }
    feedme::views::WifiSwitchView&    wifiSwitchView()    { return wifiSwitchView_; }
    feedme::domain::CatRoster&        roster()            { return roster_; }
    feedme::domain::UserRoster&       userRoster()        { return userRoster_; }
    // Per-cat tunables now live on the active cat. Callers used to
    // hold a stable reference to a global PortionState; with per-cat
    // routing the reference points at whichever cat is active. Today
    // the active cat doesn't change at runtime (no cat-selector yet
    // for N≥2), so the reference identity is stable in practice.
    feedme::domain::PortionState&   portion()      { return roster_.activePortion(); }
    feedme::domain::QuietWindow&    quiet()        { return quiet_; }
    feedme::domain::WakeTime&       wake()         { return wake_; }
    feedme::domain::TimeZone&       timezone()     { return tz_; }
    feedme::domain::SleepTimeout&   sleepTimeout() { return sleep_; }
    feedme::views::SleepTimeoutEditView& sleepTimeoutEditView() { return sleepTimeoutEditView_; }

private:
    // Multi-screen scene graph: ScreenManager owns the views, hides
    // the inactive ones, routes render() / handleInput() to the
    // current one. Each view is a static instance kept here so its
    // memory is bound to the LvglDisplay's lifetime.
    feedme::views::ScreenManager screens_;
    // Phase C.6 — boot splash, shown for ~1.2 s on power-on.
    feedme::views::BootView      bootView_;
    feedme::views::IdleView      idleView_;
    feedme::views::MenuView      menuView_;
    // Phase C.1 — the Feed flow (FeedConfirm → Pouring → Fed) plus the
    // PortionAdjust side-screen.
    feedme::views::FeedConfirmView   feedConfirmView_;
    feedme::views::PortionAdjustView portionAdjustView_;
    feedme::views::FeederPickerView  feederPickerView_;
    feedme::views::PouringView       pouringView_;
    feedme::views::FedView           fedView_;
    // Phase C.2 — read-only Schedule + the Phase D add-on editor
    // for the active cat's slot hours.
    feedme::views::ScheduleView      scheduleView_;
    feedme::views::ScheduleEditView  scheduleEditView_;
    // Phase C.3 — Quiet hours toggle.
    feedme::views::QuietView         quietView_;
    // Phase C.4 — Settings list (sub-editors land in Phase D).
    feedme::views::SettingsView      settingsView_;
    // Phase C.5 — cross-cutting Lock Confirm parental gate.
    feedme::views::LockConfirmView   lockConfirmView_;
    // Phase D.1 — Wake-time editor sub-screen.
    feedme::views::WakeTimeEditView    wakeTimeEditView_;
    // Timezone offset editor.
    feedme::views::TimeZoneEditView    timezoneEditView_;
    // Sleep-timeout (LCD backlight) editor.
    feedme::views::SleepTimeoutEditView sleepTimeoutEditView_;
    // Phase D.2 — Quiet hours start/end editor.
    feedme::views::QuietHoursEditView  quietHoursEditView_;
    // Phase D.3 — Hungry-threshold editor.
    feedme::views::ThresholdEditView   thresholdEditView_;
    // Phase D.4 — "Switch Wi-Fi" confirmation (was "reset").
    feedme::views::WifiResetView       wifiResetView_;
    // In-place AP+STA portal status while a switch is in flight.
    feedme::views::WifiSwitchView      wifiSwitchView_;
    // Phase 2.4 — captive-portal setup screen (text-only).
    feedme::views::SetupView           setupView_;
    // Phase D.5 — Cats roster + per-cat slug picker.
    feedme::views::CatsListView        catsListView_;
    feedme::views::CatEditView         catEditView_;
    feedme::views::CatRemoveView       catRemoveView_;
    // Phase D.6 — Users roster (no per-user editor in v0).
    feedme::views::UsersListView       usersListView_;
    feedme::views::UserRemoveView      userRemoveView_;
#if defined(FEEDME_HAS_HOPPER)
    // Phase C.6 — Hopper level (feature-flagged; no load cell on this
    // board, so contents are static placeholder until real hardware).
    feedme::views::HopperView        hopperView_;
#endif

    // (MealSchedule moved into Cat — per-cat now. ScheduleView reads
    // it via roster_.activeSchedule().)
    // Quiet-hours window (enabled bool only is mutable + persisted in
    // C.3; start/end times join in Phase D).
    feedme::domain::QuietWindow  quiet_;
    // Wake-time anchor (Phase D.1; default 06:30 — DEFAULT_HOUR /
    // DEFAULT_MINUTE constants in WakeTime.h).
    feedme::domain::WakeTime     wake_;
    // Cat roster (Phase D.5; defaults to 1 cat seeded on first boot).
    feedme::domain::CatRoster    roster_;
    // User roster (Phase D.6; defaults to 1 user seeded on first boot).
    feedme::domain::UserRoster   userRoster_;
    // Local timezone offset (default UTC). Display layer uses this
    // when projecting unix epoch into hour/minute for the clock face
    // and for local-hour comparisons in Schedule / Quiet.
    feedme::domain::TimeZone     tz_;

    // Display-sleep idle timeout (minutes). 0 = never. PowerManager
    // in main.cpp consumes the value and toggles the LCD backlight.
    feedme::domain::SleepTimeout sleep_;

    // The legacy LVGL-primitive cat is kept compiled (per
    // feedmeknob-plan.md open question 3 — answered "keep") but is no
    // longer in the live scene; CatFace.{h,cpp} continue to build for
    // the simulator and as a backup if PNG embedding ever becomes
    // flash-tight.
    CatFace   cat_;

    // History overlay panel (hidden by default).
    static constexpr uint32_t HISTORY_AUTO_DISMISS_MS = 10000;
    lv_obj_t* historyPanel_         = nullptr;
    lv_obj_t* historyTitle_         = nullptr;
    lv_obj_t* historyLines_[HISTORY_MAX] = {nullptr};
    bool      historyVisible_       = false;
    uint32_t  historyShownMs_       = 0;

    void buildScene();
    void buildHistoryOverlay();
};

}  // namespace feedme::adapters
