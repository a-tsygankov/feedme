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
    MealSlot slots_[SLOT_COUNT];
};

}  // namespace feedme::domain
