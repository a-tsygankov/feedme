#pragma once

#include "adapters/CatFace.h"
#include "ports/IDisplay.h"

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

    // History-overlay control.
    void setHistory(const HistoryItem* items, int count);
    void setHistoryVisible(bool visible);
    bool historyVisible() const { return historyVisible_; }

private:
    // LVGL widgets owned by us.
    lv_obj_t* arc_     = nullptr;
    lv_obj_t* face_    = nullptr;
    lv_obj_t* moodLbl_ = nullptr;
    lv_obj_t* timeLbl_ = nullptr;
    lv_obj_t* dots_[3] = {nullptr, nullptr, nullptr};
    CatFace   cat_;

    // History overlay panel (hidden by default).
    lv_obj_t* historyPanel_         = nullptr;
    lv_obj_t* historyTitle_         = nullptr;
    lv_obj_t* historyLines_[HISTORY_MAX] = {nullptr};
    bool      historyVisible_       = false;

    feedme::ports::DisplayFrame lastFrame_{};
    bool firstRender_ = true;

    void buildScene();
    void buildHistoryOverlay();
};

}  // namespace feedme::adapters
