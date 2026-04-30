#pragma once

namespace feedme::domain {

// Display-sleep idle timeout in minutes. After this many minutes
// without any input (rotate / press / tap), main.cpp turns off the
// LCD backlight to save power. Any subsequent input wakes — see
// PowerManager in main.cpp for the wake-only-on-Idle constraint.
//
// Range: 0..60 minutes in 1-minute steps.
//   0 → "never sleep" (display "--" in the editor and Settings row)
//   1..60 → minutes of inactivity before sleep
//
// What sleep does in v0: backlight off, LVGL keeps rendering (cheap
// when nothing changes), input pumps continue. The first input after
// sleep wakes the screen and is then *consumed* by the dispatcher —
// it does not propagate to the active view, so accidentally bumping
// the knob to wake doesn't also open the menu.
//
// dirty_ flags pending NVS writes — main.cpp polls consumeDirty()
// once per service tick, same pattern as TimeZone / WakeTime / etc.
class SleepTimeout {
public:
    static constexpr int DEFAULT_MIN = 5;
    static constexpr int MIN_MIN     = 0;     // 0 = never
    static constexpr int MAX_MIN     = 60;    // cap at 1 h
    static constexpr int STEP_MIN    = 1;

    int  minutes() const { return minutes_; }
    bool enabled() const { return minutes_ > 0; }

    void set(int m) {
        const int next = clamp(m);
        if (next == minutes_) return;
        minutes_ = next;
        dirty_   = true;
    }
    void bumpUp()   { set(minutes_ + STEP_MIN); }
    void bumpDown() { set(minutes_ - STEP_MIN); }

    void loadFromStorage(int m) {
        minutes_ = clamp(m);
        dirty_   = false;
    }
    bool consumeDirty() {
        const bool d = dirty_;
        dirty_ = false;
        return d;
    }

private:
    static int clamp(int v) {
        if (v < MIN_MIN) return MIN_MIN;
        if (v > MAX_MIN) return MAX_MIN;
        return v;
    }
    int  minutes_ = DEFAULT_MIN;
    bool dirty_   = false;
};

}  // namespace feedme::domain
