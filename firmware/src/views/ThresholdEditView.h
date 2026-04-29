#pragma once

#include "views/IView.h"

namespace feedme::application { class DisplayCoordinator; }

namespace feedme::views {

// Phase D.3 — Hungry-threshold editor.
//
// Single field. Rotate adjusts ±30 min (THRESHOLD_STEP_SEC). Press
// confirms and returns to Settings. Clamping + NVS persistence happen
// inside DisplayCoordinator::adjustHungryThreshold().
//
// This absorbs the dev-3 "rotate-anywhere-tunes-threshold" affordance
// — that behavior was deactivated when rotation became per-view in
// Phase B; it now lives here on its own screen.
class ThresholdEditView : public IView {
public:
    static constexpr int64_t STEP_SEC = 30 * 60;  // 30 minutes per detent

    void setCoordinator(feedme::application::DisplayCoordinator* c) { coord_ = c; }

    const char* name()   const override { return "thresholdEdit"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::application::DisplayCoordinator* coord_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* fieldLbl_  = nullptr;
    lv_obj_t* valueLbl_  = nullptr;
    lv_obj_t* unitLbl_   = nullptr;
    lv_obj_t* hint_      = nullptr;

    int64_t lastDrawnSec_ = -1;
};

}  // namespace feedme::views
