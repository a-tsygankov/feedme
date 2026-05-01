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
    // 29 today: 25 base + Setup + Pair + ResetPairConfirm + Home.
    // Bump generously — the array is just pointers (32 * 4 B on a 32-bit
    // target), and silent overflow here puts the device in a boot-loop:
    // the Settings → "destination view" lookup fails, the dispatcher
    // can't transition, and the screen freezes on whatever the previous
    // view rendered (most visibly: BootView trying to land on PairingView
    // and being stuck forever).
    static constexpr int MAX_VIEWS = 32;

    // Drop input events that arrive within this window after a screen
    // transition. Catches the case where a single press (or a quick
    // sequence of bouncy ones) ricochets into the destination view's
    // handler — e.g. press to enter Feed, the same press carries over
    // and skips FeedConfirm to whatever Press triggers next. 200 ms is
    // shorter than a deliberate user re-press but plenty of headroom
    // for any transition / debounce overlap.
    static constexpr uint32_t TRANSITION_COOLDOWN_MS = 200;

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
    uint32_t lastTransitionMs_ = 0;

    IView* find(const char* name) const;
};

}  // namespace feedme::views
