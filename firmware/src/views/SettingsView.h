#pragma once

#include "domain/QuietWindow.h"
#include "domain/SleepTimeout.h"
#include "domain/TimeZone.h"
#include "domain/WakeTime.h"
#include "ports/INetwork.h"
#include "views/IView.h"

#include <string>

namespace feedme::application { class DisplayCoordinator; }

namespace feedme::views {

// 11 Settings — vertical list with 4 items (Wi-Fi, Wake, Quiet, Calibrate).
// Selected row is centred; rows above/below fade with distance. A 140°
// accent arc on the left edge marks the selection slot.
//
// Per FeedMeKnob handoff §3:
//   knob:  rotate to scroll, press → edit
//   touch: tap row → edit
//   long-press / long-touch → menu (canonical cancel)
//
// "Edit" sub-screens (wake-time picker, Wi-Fi reset, calibration) all
// land in Phase D. For C.4, Tap/Press is a no-op so the structure ships
// without the editors.
class SettingsView : public IView {
public:
    static constexpr int ITEM_COUNT = 6;  // Wi-Fi, Wake, Notify, Threshold, Timezone, Sleep
                                          // (Cats / Users / Pair / Reset moved to HomeView)

    void setNetwork(const feedme::ports::INetwork* network) { network_ = network; }
    void setQuiet  (const feedme::domain::QuietWindow* quiet) { quiet_ = quiet; }
    void setWake   (const feedme::domain::WakeTime* wake) { wake_ = wake; }
    void setTimeZone   (const feedme::domain::TimeZone* tz) { tz_ = tz; }
    void setSleepTimeout(const feedme::domain::SleepTimeout* s) { sleep_ = s; }
    void setCoordinator(const feedme::application::DisplayCoordinator* c) { coord_ = c; }

    const char* name()   const override { return "settings"; }
    const char* parent() const override { return "menu"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    const feedme::ports::INetwork*                   network_ = nullptr;
    const feedme::domain::QuietWindow*               quiet_   = nullptr;
    const feedme::domain::WakeTime*                  wake_    = nullptr;
    const feedme::domain::TimeZone*                  tz_         = nullptr;
    const feedme::domain::SleepTimeout*              sleep_      = nullptr;
    const feedme::application::DisplayCoordinator*   coord_      = nullptr;

    lv_obj_t* root_                          = nullptr;
    lv_obj_t* selectionArc_                  = nullptr;
    lv_obj_t* rowContainers_[ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowIcons_     [ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowLabels_    [ITEM_COUNT]     = {nullptr};
    lv_obj_t* rowValues_    [ITEM_COUNT]     = {nullptr};

    int  selectedIdx_         = 0;
    int  lastDrawnIdx_        = -1;
    bool lastDrawnOnline_     = false;
    std::string lastDrawnSsid_;       // re-render Wi-Fi row when SSID changes
    bool lastDrawnQuietEnabled_ = false;
    int  lastDrawnWakeHour_   = -1;
    int  lastDrawnWakeMinute_ = -1;
    long lastDrawnThresholdSec_ = -1;
    int  lastDrawnTzMin_        = -99999;
    int  lastDrawnSleepMin_     = -99999;
    bool firstRender_         = true;
};

}  // namespace feedme::views
