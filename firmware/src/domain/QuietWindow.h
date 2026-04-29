#pragma once

namespace feedme::domain {

// Quiet-hours window. Default 22:00 → 06:30 (wraps midnight). Phase
// D.2 makes start/end editable and persisted; the enabled bool was
// already mutable as of C.3.
//
// Minute fields snap to MINUTE_STEP (5) so the knob editor doesn't
// need 60 detents per hour-pass. `dirty_` flags pending NVS writes
// covering enabled + all four time fields atomically — main.cpp
// re-persists the lot on the next service tick after any change.
class QuietWindow {
public:
    static constexpr int DEFAULT_START_HOUR   = 22;
    static constexpr int DEFAULT_START_MINUTE = 0;
    static constexpr int DEFAULT_END_HOUR     = 6;
    static constexpr int DEFAULT_END_MINUTE   = 30;
    static constexpr int MINUTE_STEP          = 5;

    bool enabled()     const { return enabled_; }
    int  startHour()   const { return startHour_; }
    int  startMinute() const { return startMinute_; }
    int  endHour()     const { return endHour_; }
    int  endMinute()   const { return endMinute_; }

    void setEnabled(bool e) {
        if (enabled_ == e) return;
        enabled_ = e;
        dirty_   = true;
    }
    void toggle() { setEnabled(!enabled_); }

    void setStartHour(int h)   { setHour(startHour_, h); }
    void setStartMinute(int m) { setMinute(startMinute_, m); }
    void setEndHour(int h)     { setHour(endHour_, h); }
    void setEndMinute(int m)   { setMinute(endMinute_, m); }

    void bumpStartHour(int delta)   { setHour(startHour_, wrap(startHour_ + delta, 24)); }
    void bumpStartMinute(int delta) { setMinute(startMinute_, wrap(startMinute_ + delta * MINUTE_STEP, 60)); }
    void bumpEndHour(int delta)     { setHour(endHour_, wrap(endHour_ + delta, 24)); }
    void bumpEndMinute(int delta)   { setMinute(endMinute_, wrap(endMinute_ + delta * MINUTE_STEP, 60)); }

    void loadFromStorage(bool e, int sh, int sm, int eh, int em) {
        enabled_     = e;
        startHour_   = clampHour(sh);
        startMinute_ = clampMinute(sm);
        endHour_     = clampHour(eh);
        endMinute_   = clampMinute(em);
        dirty_       = false;
    }
    bool consumeDirty() {
        const bool d = dirty_;
        dirty_ = false;
        return d;
    }

    // True if the given local hour:minute falls inside the (always-
    // computed, regardless of enabled) window.
    //
    // Two shapes:
    //   - Wrap-midnight   (start > end, e.g. 22:00 → 06:30):
    //       contained iff  now >= start  OR  now < end
    //   - Same-day        (start <= end, e.g. 10:00 → 14:00):
    //       contained iff  now >= start  AND  now < end
    //
    // start == end is treated as "empty window" (never contained) to
    // match the editor's UX — saving the same time on both ends should
    // be the same as turning quiet hours off, not "always on".
    //
    // Both endpoints use [start, end) — the start minute counts as
    // inside, the end minute counts as outside. Matches the user
    // intuition of "until 06:30" meaning quiet up to but not including
    // 06:30.
    bool contains(int hour, int minute) const {
        const int now   = hour * 60 + minute;
        const int start = startHour_ * 60 + startMinute_;
        const int end   = endHour_   * 60 + endMinute_;
        if (start == end) return false;
        if (start > end)  return now >= start || now < end;   // wraps midnight
        return now >= start && now < end;                      // same-day window
    }

private:
    void setHour(int& field, int v) {
        const int next = clampHour(v);
        if (next == field) return;
        field  = next;
        dirty_ = true;
    }
    void setMinute(int& field, int v) {
        const int next = clampMinute(v);
        if (next == field) return;
        field  = next;
        dirty_ = true;
    }
    static int clampHour(int v)   { return wrap(v, 24); }
    static int clampMinute(int v) {
        const int snapped = (v / MINUTE_STEP) * MINUTE_STEP;
        return wrap(snapped, 60);
    }
    static int wrap(int v, int mod) {
        v %= mod;
        if (v < 0) v += mod;
        return v;
    }

    bool enabled_     = false;
    int  startHour_   = DEFAULT_START_HOUR;
    int  startMinute_ = DEFAULT_START_MINUTE;
    int  endHour_     = DEFAULT_END_HOUR;
    int  endMinute_   = DEFAULT_END_MINUTE;
    bool dirty_       = false;
};

}  // namespace feedme::domain
