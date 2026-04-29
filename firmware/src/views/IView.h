#pragma once

#include "ports/IDisplay.h"
#include "ports/ITapSensor.h"

#include <lvgl.h>

namespace feedme::views {

// One screen on the round display. Owns its LVGL widgets which live as
// children of a parent `lv_obj_t*` provided at build() time. Active /
// inactive views are toggled via root visibility — widgets aren't
// destroyed on transition, just hidden, so re-entry is instant.
//
// Responsibility split:
//   - build(parent)        construct widgets once, hide them
//   - onEnter()            show widgets, reset transient state
//   - onLeave()            hide widgets
//   - render(frame)        push fresh data into the widgets
//   - handleInput(ev)      gesture → next view or in-view mutation
//
// handleInput returns the *next* view name, or nullptr to stay.
class IView {
public:
    virtual ~IView() = default;

    virtual const char* name() const = 0;
    virtual void  build(lv_obj_t* parent) = 0;
    virtual void  onEnter() = 0;
    virtual void  onLeave() = 0;
    virtual void  render(const feedme::ports::DisplayFrame& frame) = 0;

    // Returns the name of the next view ("idle", "menu", "feedConfirm" ...)
    // or nullptr to remain on the current view. Caller transitions.
    virtual const char* handleInput(feedme::ports::TapEvent ev) = 0;

    // Time-driven self-transition. Polled by ScreenManager after render().
    // Default: never. Used by Pouring (→ Fed on animation complete) and
    // Fed (→ Idle on auto-dismiss).
    virtual const char* nextView() { return nullptr; }
};

}  // namespace feedme::views
