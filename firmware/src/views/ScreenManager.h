#pragma once

#include "views/IView.h"

#include <lvgl.h>

#include <stdint.h>

namespace feedme::views {

// Owns up to N IView instances and routes render() / input events to
// the active one. Transitions hide the previous view's root and show
// the new view's root — no widget re-construction.
//
// N is intentionally fixed (no heap) — each view is registered as a
// pointer to a stack/static instance owned by LvglDisplay.
class ScreenManager {
public:
    static constexpr int MAX_VIEWS = 16;

    void begin(lv_obj_t* parent);
    void registerView(IView* view);  // call once per view in any order
    void transition(const char* name);  // by IView::name()

    IView* current() const { return current_; }
    void   render(const feedme::ports::DisplayFrame& frame);
    const char* handleInput(feedme::ports::TapEvent ev);

private:
    IView*   views_[MAX_VIEWS] = {nullptr};
    int      viewCount_ = 0;
    IView*   current_   = nullptr;
    lv_obj_t* parent_   = nullptr;

    IView* find(const char* name) const;
};

}  // namespace feedme::views
