#pragma once

namespace feedme::domain {

// Wake time = the household's morning anchor. Currently just shown on
// the Settings list (and implicitly informs "next 13:00 lunch"-style
// labels in the FeedMeKnob workflows). Hour 0..23, minute in 5-min
// detents (so the knob editor doesn't need 60 detents per hour-pass).
//
// `dirty_` flags pending NVS writes; main.cpp polls consumeDirty()
// once per service tick.
class WakeTime {
public:
    static constexpr int DEFAULT_HOUR   = 6;
    static constexpr int DEFAULT_MINUTE = 30;
    static constexpr int MINUTE_STEP    = 5;

    int hour()   const { return hour_; }
    int minute() const { return minute_; }

    void setHour(int h) {
        const int next = clampHour(h);
        if (next == hour_) return;
        hour_ = next;
        dirty_ = true;
    }
    void setMinute(int m) {
        const int next = clampMinute(m);
        if (next == minute_) return;
        minute_ = next;
        dirty_ = true;
    }
    void bumpHour(int delta)   { setHour(wrap(hour_ + delta, 24)); }
    void bumpMinute(int delta) {
        // Snap step before clamp so rotation always moves by MINUTE_STEP.
        setMinute(wrap(minute_ + delta * MINUTE_STEP, 60));
    }

    void loadFromStorage(int h, int m) {
        hour_   = clampHour(h);
        minute_ = clampMinute(m);
        dirty_  = false;
    }
    bool consumeDirty() {
        const bool d = dirty_;
        dirty_ = false;
        return d;
    }

private:
    static int clampHour(int h)   { return wrap(h, 24); }
    static int clampMinute(int m) {
        // Snap to the nearest MINUTE_STEP so callers can't poke odd
        // values via bad NVS data.
        const int snapped = (m / MINUTE_STEP) * MINUTE_STEP;
        return wrap(snapped, 60);
    }
    static int wrap(int v, int mod) {
        v %= mod;
        if (v < 0) v += mod;
        return v;
    }

    int  hour_   = DEFAULT_HOUR;
    int  minute_ = DEFAULT_MINUTE;
    bool dirty_  = false;
};

}  // namespace feedme::domain
