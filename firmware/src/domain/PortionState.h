#pragma once

namespace feedme::domain {

// Default-meal-size portion in grams, with clamp + step constants per
// the FeedMeKnob design (default 40 g, range 5–60, ±5 g per detent).
//
// Shared mutable state between FeedConfirmView (rotate to adjust) and
// PortionAdjustView (dedicated big-number editor). LvglDisplay owns one
// instance; the views hold a pointer.
//
// `dirty_` is the "needs persistence" flag — main.cpp polls
// consumeDirty() once per service tick and writes through to NVS so we
// don't hammer flash on every detent.
class PortionState {
public:
    static constexpr int MIN_G     = 5;
    static constexpr int MAX_G     = 60;
    static constexpr int STEP_G    = 5;
    static constexpr int DEFAULT_G = 40;

    explicit PortionState(int initial = DEFAULT_G) {
        grams_ = clamp(initial);
    }

    int  grams() const { return grams_; }

    // Returns true if the value actually changed.
    bool set(int g) {
        const int next = clamp(g);
        if (next == grams_) return false;
        grams_ = next;
        dirty_ = true;
        return true;
    }
    bool bumpUp()   { return set(grams_ + STEP_G); }
    bool bumpDown() { return set(grams_ - STEP_G); }

    // Used by main.cpp to decide whether to persist this tick.
    bool consumeDirty() {
        const bool d = dirty_;
        dirty_ = false;
        return d;
    }

    // Used by loadPreferences() to seed without flagging dirty.
    void loadFromStorage(int g) {
        grams_ = clamp(g);
        dirty_ = false;
    }

private:
    static int clamp(int g) {
        if (g < MIN_G) return MIN_G;
        if (g > MAX_G) return MAX_G;
        return g;
    }

    int  grams_ = DEFAULT_G;
    bool dirty_ = false;
};

}  // namespace feedme::domain
