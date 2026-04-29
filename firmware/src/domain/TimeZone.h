#pragma once

namespace feedme::domain {

// Local timezone offset from UTC, in minutes. Signed: negative for
// timezones west of UTC (e.g. America/New_York EST = -300), positive
// for east (e.g. India = +330).
//
// What's stored / what's not:
//   - All event timestamps (lastFeedTs, history, backend) are UTC
//     unix seconds — never converted at storage time.
//   - All "stored hour" values (wake time, quiet start/end, schedule
//     slot hours) are LOCAL TIME by the user's intent — typing "06:30
//     wake" means local 06:30. They're compared against the local
//     hour computed from now+offset by DisplayCoordinator.
//   - Mood / ring / "fed Xm ago" math is pure timestamp deltas —
//     timezone-agnostic.
//
// Resolution: hour only for v0 (range -12..+14). Half-hour zones
// (India, Newfoundland) and 45-minute zones (Nepal, Chatham) want
// finer steps; bump MIN/MAX/STEP later when the editor needs them.
class TimeZone {
public:
    static constexpr int DEFAULT_MIN = 0;       // UTC
    static constexpr int MIN_MIN     = -12 * 60;
    static constexpr int MAX_MIN     = +14 * 60;
    static constexpr int STEP_MIN    = 60;       // hour-resolution editor

    int  offsetMin() const { return offsetMin_; }
    int  offsetSec() const { return offsetMin_ * 60; }

    void set(int minutes) {
        const int next = clamp(minutes);
        if (next == offsetMin_) return;
        offsetMin_ = next;
        dirty_     = true;
    }
    void bumpHour(int delta) { set(offsetMin_ + delta * STEP_MIN); }

    void loadFromStorage(int minutes) {
        offsetMin_ = clamp(minutes);
        dirty_     = false;
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
    int  offsetMin_ = DEFAULT_MIN;
    bool dirty_     = false;
};

}  // namespace feedme::domain
