#pragma once

namespace feedme::domain {

struct MealSlot {
    int         hour;     // 0..23
    const char* label;    // "Breakfast" / "Lunch" / "Dinner" / "Treat"
};

// Four meal slots laid out around the perimeter at 12/3/6/9 o'clock.
// Hours hardcoded for C.2 (read-only); persistence + the editor land
// in Phase D.x. The hour-based "served / current" model below also
// has no correlation to actual feed events — a slot is "served" iff
// the wall clock has passed its hour. Wiring it to FeedingService
// history is a follow-up once the schedule editor is interactive.
class MealSchedule {
public:
    static constexpr int SLOT_COUNT = 4;

    MealSchedule()
        : slots_{ { 7, "Breakfast"},
                  {13, "Lunch"},
                  {18, "Dinner"},
                  {22, "Treat"} } {}

    const MealSlot& slot(int i) const { return slots_[i]; }
    int             slotHour(int i) const {
        return (i >= 0 && i < SLOT_COUNT) ? slots_[i].hour : -1;
    }

    // Returns true if the value actually changed (so callers can
    // gate dirty-flag propagation upstream).
    bool setSlotHour(int i, int hour) {
        if (i < 0 || i >= SLOT_COUNT) return false;
        const int wrapped = wrap(hour, 24);
        if (slots_[i].hour == wrapped) return false;
        slots_[i].hour = wrapped;
        return true;
    }
    bool bumpSlotHour(int i, int delta) {
        if (i < 0 || i >= SLOT_COUNT) return false;
        return setSlotHour(i, slots_[i].hour + delta);
    }

    // Index of the "next upcoming" (or current-hour) meal. Wraps to
    // slot 0 when all of today's slots have passed (post-22:00 → next
    // is tomorrow's breakfast).
    int currentSlot(int currentHour) const {
        for (int i = 0; i < SLOT_COUNT; ++i) {
            if (currentHour <= slots_[i].hour) return i;
        }
        return 0;
    }

    bool isServed(int slotIdx, int currentHour) const {
        return currentHour > slots_[slotIdx].hour;
    }

private:
    static int wrap(int v, int mod) {
        v %= mod;
        if (v < 0) v += mod;
        return v;
    }
    MealSlot slots_[SLOT_COUNT];
};

}  // namespace feedme::domain
