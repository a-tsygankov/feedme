#pragma once

#include "domain/SleepTimeout.h"
#include "views/IView.h"

namespace feedme::views {

// Sleep-timeout editor.
//
// Single field: minutes of inactivity before the LCD backlight
// turns off. Rotate adjusts ±1 min (range 0..60). Special-cases:
//   0  → displays "--" with helper line "off" (sleep disabled)
//   1+ → displays "<n>min" with helper line "of inactivity"
//
// Press / Tap → returns to Settings. Mutations apply live via
// SleepTimeout::set(); main.cpp's tick loop persists when dirty.
//
// LongPress / LongTouch → ScreenManager fallback to parent (settings).
class SleepTimeoutEditView : public IView {
public:
    void setSleepTimeout(feedme::domain::SleepTimeout* s) { sleep_ = s; }

    const char* name()   const override { return "sleepTimeoutEdit"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::domain::SleepTimeout* sleep_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* fieldLbl_  = nullptr;   // "SLEEP"
    lv_obj_t* valueLbl_  = nullptr;   // big number "5" or "--"
    lv_obj_t* unitLbl_   = nullptr;   // "MIN" or "OFF"
    lv_obj_t* hint_      = nullptr;   // bottom gesture hint

    int lastDrawnMin_ = -1;
};

}  // namespace feedme::views
