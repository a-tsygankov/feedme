#pragma once

#include "domain/TimeZone.h"
#include "views/IView.h"

namespace feedme::views {

// Timezone offset editor. Single field — UTC offset in hours, signed.
// Rotate adjusts ±1 hour, press saves and returns to Settings.
//
// Display format: "UTC", "UTC+5", "UTC-8" — sign is omitted at zero
// and prepended for non-zero offsets. Half-hour and 45-min zones
// (India, Newfoundland, Nepal) need finer resolution; bump
// TimeZone::STEP_MIN and add a minute field when that comes up.
class TimeZoneEditView : public IView {
public:
    void setTimeZone(feedme::domain::TimeZone* tz) { tz_ = tz; }

    const char* name()   const override { return "timezoneEdit"; }
    const char* parent() const override { return "settings"; }
    void  build(lv_obj_t* parent) override;
    void  onEnter() override;
    void  onLeave() override;
    void  render(const feedme::ports::DisplayFrame& frame) override;
    const char* handleInput(feedme::ports::TapEvent ev) override;

private:
    void redraw();

    feedme::domain::TimeZone* tz_ = nullptr;

    lv_obj_t* root_      = nullptr;
    lv_obj_t* fieldLbl_  = nullptr;
    lv_obj_t* valueLbl_  = nullptr;
    lv_obj_t* hint_      = nullptr;

    int lastDrawnMin_ = INT32_MIN;
};

}  // namespace feedme::views
