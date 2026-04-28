#pragma once

#include "adapters/CatFace.h"
#include "ports/IClock.h"
#include "ports/IDisplay.h"
#include "ports/ITapSensor.h"
#include "views/IdleView.h"
#include "views/MenuView.h"
#include "views/ScreenManager.h"

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

private:
    // Multi-screen scene graph: ScreenManager owns the views, hides
    // the inactive ones, routes render() / handleInput() to the
    // current one. Each view is a static instance kept here so its
    // memory is bound to the LvglDisplay's lifetime.
    feedme::views::ScreenManager screens_;
    feedme::views::IdleView      idleView_;
    feedme::views::MenuView      menuView_;

    // The legacy LVGL-primitive cat is kept compiled (per
    // feedmeknob-plan.md open question 3 — answered "keep") but is no
    // longer in the live scene; CatFace.{h,cpp} continue to build for
    // the simulator and as a backup if PNG embedding ever becomes
    // flash-tight.
    CatFace   cat_;

    // History overlay panel (hidden by default).
    lv_obj_t* historyPanel_         = nullptr;
    lv_obj_t* historyTitle_         = nullptr;
    lv_obj_t* historyLines_[HISTORY_MAX] = {nullptr};
    bool      historyVisible_       = false;

    void buildScene();
    void buildHistoryOverlay();
};

}  // namespace feedme::adapters
