#include <unity.h>

#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

#include "domain/CatRoster.h"
#include "domain/EventId.h"
#include "domain/FeedingState.h"
#include "domain/MealSchedule.h"
#include "domain/Mood.h"
#include "domain/MoodCalculator.h"
#include "domain/PortionState.h"
#include "domain/QuietWindow.h"
#include "domain/RingProgress.h"
#include "domain/SleepTimeout.h"
#include "domain/TimeZone.h"
#include "domain/UserRoster.h"
#include "domain/WakeTime.h"

using namespace feedme::domain;

static constexpr int64_t THRESHOLD = 5 * 3600;  // 5 hours, the design default

void setUp() {}
void tearDown() {}

// ── MoodCalculator ────────────────────────────────────────────────────────

void test_mood_just_fed_overrides_everything() {
    FeedingState s{};
    s.lastFeedTs = 0;          // would normally be Hungry
    s.snoozeUntilTs = 1000000;  // would normally be Sleepy
    s.justFed = true;
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Fed),
                      static_cast<int>(calculateMood(s, 100, THRESHOLD)));
}

void test_mood_snooze_overrides_age() {
    FeedingState s{};
    s.lastFeedTs = 0;          // would be Hungry
    s.snoozeUntilTs = 200;
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Sleepy),
                      static_cast<int>(calculateMood(s, 100, THRESHOLD)));
}

void test_mood_snooze_expired_falls_through() {
    FeedingState s{};
    s.lastFeedTs = 0;          // never fed
    s.snoozeUntilTs = 50;       // expired
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Hungry),
                      static_cast<int>(calculateMood(s, 100, THRESHOLD)));
}

void test_mood_never_fed_is_hungry() {
    FeedingState s{};
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Hungry),
                      static_cast<int>(calculateMood(s, 1000, THRESHOLD)));
}

void test_mood_happy_when_recently_fed() {
    FeedingState s{}; s.lastFeedTs = 1000;
    // 1 hour after feed (< threshold/2 = 2.5h)
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Happy),
                      static_cast<int>(calculateMood(s, 1000 + 3600, THRESHOLD)));
}

void test_mood_neutral_in_second_quartile() {
    FeedingState s{}; s.lastFeedTs = 1000;
    // 3 hours after feed → in [2.5h, 3.75h)
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Neutral),
                      static_cast<int>(calculateMood(s, 1000 + 3 * 3600, THRESHOLD)));
}

void test_mood_warning_in_third_quartile() {
    FeedingState s{}; s.lastFeedTs = 1000;
    // 4 hours after feed → in [3.75h, 5h)
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Warning),
                      static_cast<int>(calculateMood(s, 1000 + 4 * 3600, THRESHOLD)));
}

void test_mood_hungry_past_threshold() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Hungry),
                      static_cast<int>(calculateMood(s, 1000 + 6 * 3600, THRESHOLD)));
}

void test_mood_clock_went_backwards_is_happy() {
    FeedingState s{}; s.lastFeedTs = 10000;
    TEST_ASSERT_EQUAL(static_cast<int>(Mood::Happy),
                      static_cast<int>(calculateMood(s, 9000, THRESHOLD)));
}

// ── RingProgress ──────────────────────────────────────────────────────────

void test_ring_full_when_just_fed() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                             computeRingProgress(s, 1000, THRESHOLD));
}

void test_ring_half_at_half_threshold() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f,
                             computeRingProgress(s, 1000 + THRESHOLD / 2, THRESHOLD));
}

void test_ring_empty_at_threshold() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                             computeRingProgress(s, 1000 + THRESHOLD, THRESHOLD));
}

void test_ring_empty_past_threshold() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                             computeRingProgress(s, 1000 + 2 * THRESHOLD, THRESHOLD));
}

void test_ring_zero_when_never_fed() {
    FeedingState s{};
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                             computeRingProgress(s, 1000, THRESHOLD));
}

void test_ring_full_when_clock_backwards() {
    FeedingState s{}; s.lastFeedTs = 10000;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f,
                             computeRingProgress(s, 9000, THRESHOLD));
}

void test_ring_handles_zero_threshold_safely() {
    FeedingState s{}; s.lastFeedTs = 1000;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f,
                             computeRingProgress(s, 1000, 0));
}

// ── TimeZone ──────────────────────────────────────────────────────────────

void test_tz_default_is_utc() {
    TimeZone tz;
    TEST_ASSERT_EQUAL_INT(0, tz.offsetMin());
    TEST_ASSERT_EQUAL_INT(0, tz.offsetSec());
}

void test_tz_set_marks_dirty_and_clamps() {
    TimeZone tz;
    tz.set(330);  // India = UTC+5:30
    TEST_ASSERT_EQUAL_INT(330, tz.offsetMin());
    TEST_ASSERT_EQUAL_INT(330 * 60, tz.offsetSec());
    TEST_ASSERT_TRUE(tz.consumeDirty());
    TEST_ASSERT_FALSE(tz.consumeDirty());  // cleared after first read
}

void test_tz_set_same_value_doesnt_mark_dirty() {
    TimeZone tz;
    tz.set(0);  // already UTC
    TEST_ASSERT_FALSE(tz.consumeDirty());
}

void test_tz_clamps_below_min() {
    TimeZone tz;
    tz.set(-9999);
    TEST_ASSERT_EQUAL_INT(TimeZone::MIN_MIN, tz.offsetMin());
}

void test_tz_clamps_above_max() {
    TimeZone tz;
    tz.set(9999);
    TEST_ASSERT_EQUAL_INT(TimeZone::MAX_MIN, tz.offsetMin());
}

void test_tz_bump_hour_positive() {
    TimeZone tz;
    tz.bumpHour(3);
    TEST_ASSERT_EQUAL_INT(180, tz.offsetMin());
}

void test_tz_bump_hour_negative_past_min_clamps() {
    TimeZone tz;
    tz.bumpHour(-100);  // would be -6000, clamps to -720
    TEST_ASSERT_EQUAL_INT(TimeZone::MIN_MIN, tz.offsetMin());
}

void test_tz_load_from_storage_doesnt_mark_dirty() {
    TimeZone tz;
    tz.loadFromStorage(-300);  // EST
    TEST_ASSERT_EQUAL_INT(-300, tz.offsetMin());
    TEST_ASSERT_FALSE(tz.consumeDirty());
}

// ── CatRoster ─────────────────────────────────────────────────────────────

void test_cat_roster_seed_default_creates_one_cat() {
    CatRoster r;
    r.seedDefaultIfEmpty();
    TEST_ASSERT_EQUAL_INT(1, r.count());
    TEST_ASSERT_EQUAL_INT(0, r.at(0).id);
}

void test_cat_roster_seed_default_idempotent() {
    CatRoster r;
    r.seedDefaultIfEmpty();
    r.seedDefaultIfEmpty();
    TEST_ASSERT_EQUAL_INT(1, r.count());
}

void test_cat_roster_add_assigns_climbing_ids() {
    CatRoster r;
    int s0 = r.add();
    int s1 = r.add();
    int s2 = r.add();
    TEST_ASSERT_EQUAL_INT(0, s0);
    TEST_ASSERT_EQUAL_INT(1, s1);
    TEST_ASSERT_EQUAL_INT(2, s2);
    TEST_ASSERT_EQUAL_INT(0, r.at(0).id);
    TEST_ASSERT_EQUAL_INT(1, r.at(1).id);
    TEST_ASSERT_EQUAL_INT(2, r.at(2).id);
}

void test_cat_roster_add_caps_at_max() {
    CatRoster r;
    for (int i = 0; i < CatRoster::MAX_CATS; ++i) r.add();
    TEST_ASSERT_EQUAL_INT(-1, r.add());  // refuses
    TEST_ASSERT_EQUAL_INT(CatRoster::MAX_CATS, r.count());
}

void test_cat_roster_remove_refuses_last() {
    CatRoster r;
    r.add();
    TEST_ASSERT_FALSE(r.remove(0));
    TEST_ASSERT_EQUAL_INT(1, r.count());
}

void test_cat_roster_remove_shifts_indices() {
    CatRoster r;
    r.add(); r.add(); r.add();   // ids 0,1,2 in slots 0,1,2
    TEST_ASSERT_TRUE(r.remove(1));  // remove middle
    TEST_ASSERT_EQUAL_INT(2, r.count());
    TEST_ASSERT_EQUAL_INT(0, r.at(0).id);  // was slot 0
    TEST_ASSERT_EQUAL_INT(2, r.at(1).id);  // shifted from slot 2 → 1
}

void test_cat_roster_remove_preserves_next_id() {
    CatRoster r;
    r.add(); r.add(); r.add();   // nextId is now 3
    r.remove(0);                  // ids {1,2}, nextId still 3
    int newSlot = r.add();
    TEST_ASSERT_EQUAL_INT(3, r.at(newSlot).id);  // never reuses removed id 0
}

void test_cat_roster_remove_clamps_active_idx() {
    CatRoster r;
    r.add(); r.add(); r.add();
    r.setActiveCatIdx(2);
    r.remove(2);
    // active was on the removed slot — clamps down to last existing
    TEST_ASSERT_EQUAL_INT(1, r.activeCatIdx());
}

void test_cat_roster_remove_resets_feed_selection_when_out_of_range() {
    CatRoster r;
    r.add(); r.add(); r.add();
    r.setFeedSelection(2);  // valid for 3 cats
    r.remove(2);            // now 2 cats; selection 2 invalid
    TEST_ASSERT_EQUAL_INT(CatRoster::FEED_ALL, r.feedSelection());
}

void test_cat_roster_remove_marks_dirty() {
    CatRoster r;
    r.add(); r.add();
    r.consumeDirty();  // clear dirty from add
    r.remove(0);
    TEST_ASSERT_TRUE(r.consumeDirty());
}

void test_cat_roster_find_slot_by_id() {
    CatRoster r;
    r.add(); r.add(); r.add();
    r.remove(0);  // surviving ids are 1, 2 in slots 0, 1
    TEST_ASSERT_EQUAL_INT(0, r.findSlotById(1));
    TEST_ASSERT_EQUAL_INT(1, r.findSlotById(2));
    TEST_ASSERT_EQUAL_INT(-1, r.findSlotById(0));   // removed
    TEST_ASSERT_EQUAL_INT(-1, r.findSlotById(99));  // never existed
}

void test_cat_roster_feed_selection_rejects_out_of_range() {
    CatRoster r;
    r.add(); r.add();
    r.setFeedSelection(5);  // out of range
    TEST_ASSERT_EQUAL_INT(CatRoster::FEED_ALL, r.feedSelection());  // unchanged
    r.setFeedSelection(1);
    TEST_ASSERT_EQUAL_INT(1, r.feedSelection());
    r.setFeedSelection(CatRoster::FEED_ALL);
    TEST_ASSERT_EQUAL_INT(CatRoster::FEED_ALL, r.feedSelection());
}

void test_cat_roster_append_loaded_seeds_next_id() {
    CatRoster r;
    r.appendLoaded(5, "Whiskers", "C2");
    r.appendLoaded(7, "Mittens", "C3");
    int newSlot = r.add();
    // nextId should be max(loaded) + 1 = 8
    TEST_ASSERT_EQUAL_INT(8, r.at(newSlot).id);
}

void test_cat_roster_active_threshold_dirty_only_on_change() {
    CatRoster r;
    r.add();
    r.consumeDirty();
    r.setActiveThresholdSec(Cat::DEFAULT_THRESHOLD_S);  // same value
    TEST_ASSERT_FALSE(r.consumeDirty());
    r.setActiveThresholdSec(7200);
    TEST_ASSERT_TRUE(r.consumeDirty());
    TEST_ASSERT_EQUAL_INT64(7200, r.activeThresholdSec());
}

// ── UserRoster ────────────────────────────────────────────────────────────

void test_user_roster_primary_name_falls_back_to_you() {
    UserRoster r;
    TEST_ASSERT_EQUAL_STRING("you", r.primaryName());
}

void test_user_roster_primary_name_uses_first_user() {
    UserRoster r;
    r.add();
    r.setName(0, "Alice");
    TEST_ASSERT_EQUAL_STRING("Alice", r.primaryName());
}

void test_user_roster_current_feeder_name_falls_back_to_primary() {
    UserRoster r;
    r.add(); r.setName(0, "Alice");
    r.add(); r.setName(1, "Bob");
    // No picker set → currentFeederName is the primary
    TEST_ASSERT_EQUAL_STRING("Alice", r.currentFeederName());
}

void test_user_roster_current_feeder_name_uses_picker() {
    UserRoster r;
    r.add(); r.setName(0, "Alice");
    r.add(); r.setName(1, "Bob");
    r.setCurrentFeeder(1);
    TEST_ASSERT_EQUAL_STRING("Bob", r.currentFeederName());
}

void test_user_roster_clear_current_feeder_reverts_to_primary() {
    UserRoster r;
    r.add(); r.setName(0, "Alice");
    r.add(); r.setName(1, "Bob");
    r.setCurrentFeeder(1);
    r.clearCurrentFeeder();
    TEST_ASSERT_EQUAL_STRING("Alice", r.currentFeederName());
}

void test_user_roster_set_current_feeder_rejects_out_of_range() {
    UserRoster r;
    r.add(); r.setName(0, "Alice");
    r.setCurrentFeeder(5);  // invalid; ignored
    TEST_ASSERT_EQUAL_INT(-1, r.currentFeederIdx());
    TEST_ASSERT_EQUAL_STRING("Alice", r.currentFeederName());
}

void test_user_roster_picker_is_not_dirty_tracked() {
    UserRoster r;
    r.add();
    r.consumeDirty();  // clear add-dirty
    r.setCurrentFeeder(0);
    r.clearCurrentFeeder();
    TEST_ASSERT_FALSE(r.consumeDirty());  // transient state — never persisted
}

// ── MealSchedule ──────────────────────────────────────────────────────────

void test_meal_schedule_current_slot_morning() {
    MealSchedule s;  // defaults: 7, 13, 18, 22
    TEST_ASSERT_EQUAL_INT(0, s.currentSlot(6));   // before breakfast
    TEST_ASSERT_EQUAL_INT(0, s.currentSlot(7));   // at breakfast hour
}

void test_meal_schedule_current_slot_afternoon() {
    MealSchedule s;
    TEST_ASSERT_EQUAL_INT(1, s.currentSlot(10));  // between breakfast & lunch → next is lunch
    TEST_ASSERT_EQUAL_INT(2, s.currentSlot(15));  // between lunch & dinner → next is dinner
}

void test_meal_schedule_current_slot_wraps_post_treat() {
    MealSchedule s;
    // Post-22:00 wraps to slot 0 (tomorrow's breakfast)
    TEST_ASSERT_EQUAL_INT(0, s.currentSlot(23));
}

void test_meal_schedule_set_slot_hour_wraps() {
    MealSchedule s;
    s.setSlotHour(0, 26);  // wraps to 2
    TEST_ASSERT_EQUAL_INT(2, s.slotHour(0));
    s.setSlotHour(0, -1);  // wraps to 23
    TEST_ASSERT_EQUAL_INT(23, s.slotHour(0));
}

void test_meal_schedule_set_returns_false_when_unchanged() {
    MealSchedule s;
    TEST_ASSERT_FALSE(s.setSlotHour(0, 7));  // already 7
    TEST_ASSERT_TRUE(s.setSlotHour(0, 8));   // changed
}

void test_meal_schedule_is_served_strict() {
    MealSchedule s;  // breakfast=7
    TEST_ASSERT_FALSE(s.isServed(0, 7));  // at the hour, not yet "past"
    TEST_ASSERT_TRUE(s.isServed(0, 8));
    TEST_ASSERT_FALSE(s.isServed(0, 6));
}

void test_meal_schedule_invalid_slot_idx() {
    MealSchedule s;
    TEST_ASSERT_EQUAL_INT(-1, s.slotHour(-1));
    TEST_ASSERT_EQUAL_INT(-1, s.slotHour(99));
    TEST_ASSERT_FALSE(s.setSlotHour(-1, 5));
    TEST_ASSERT_FALSE(s.bumpSlotHour(99, 1));
}

// ── QuietWindow ───────────────────────────────────────────────────────────

void test_quiet_window_defaults_disabled() {
    QuietWindow q;
    TEST_ASSERT_FALSE(q.enabled());
    TEST_ASSERT_EQUAL_INT(22, q.startHour());
    TEST_ASSERT_EQUAL_INT(0,  q.startMinute());
    TEST_ASSERT_EQUAL_INT(6,  q.endHour());
    TEST_ASSERT_EQUAL_INT(30, q.endMinute());
}

void test_quiet_window_contains_during_evening() {
    QuietWindow q;  // 22:00 → 06:30 (wraps midnight)
    TEST_ASSERT_TRUE(q.contains(22, 0));    // at start
    TEST_ASSERT_TRUE(q.contains(23, 30));   // before midnight
}

void test_quiet_window_contains_during_overnight() {
    QuietWindow q;
    TEST_ASSERT_TRUE(q.contains(0, 0));     // midnight
    TEST_ASSERT_TRUE(q.contains(3, 15));    // dead of night
    TEST_ASSERT_TRUE(q.contains(6, 29));    // just before end
}

void test_quiet_window_contains_excludes_daytime() {
    QuietWindow q;
    TEST_ASSERT_FALSE(q.contains(6, 30));   // exactly at end → not contained
    TEST_ASSERT_FALSE(q.contains(12, 0));   // noon
    TEST_ASSERT_FALSE(q.contains(21, 59));  // just before start
}

void test_quiet_window_minute_step_snaps() {
    QuietWindow q;
    q.setStartMinute(17);  // not on a 5-min boundary
    TEST_ASSERT_EQUAL_INT(15, q.startMinute());  // snapped down
    q.setStartMinute(18);
    TEST_ASSERT_EQUAL_INT(15, q.startMinute());  // 18/5*5 = 15
    q.setStartMinute(20);
    TEST_ASSERT_EQUAL_INT(20, q.startMinute());
}

void test_quiet_window_toggle_marks_dirty() {
    QuietWindow q;
    q.toggle();
    TEST_ASSERT_TRUE(q.enabled());
    TEST_ASSERT_TRUE(q.consumeDirty());
    q.toggle();
    TEST_ASSERT_FALSE(q.enabled());
    TEST_ASSERT_TRUE(q.consumeDirty());
}

void test_quiet_window_load_clears_dirty() {
    QuietWindow q;
    q.loadFromStorage(true, 23, 0, 7, 0);
    TEST_ASSERT_TRUE(q.enabled());
    TEST_ASSERT_EQUAL_INT(23, q.startHour());
    TEST_ASSERT_EQUAL_INT(7, q.endHour());
    TEST_ASSERT_FALSE(q.consumeDirty());
}

void test_quiet_window_bump_hour_wraps() {
    QuietWindow q;
    q.loadFromStorage(false, 23, 0, 6, 30);
    q.bumpStartHour(2);  // 23 + 2 = 25 → 1
    TEST_ASSERT_EQUAL_INT(1, q.startHour());
}

// Non-wrap (same-day) window: 10:00 → 14:00 is "quiet during midday",
// e.g. nap time. Must NOT report quiet at 20:00 — that's the bug the
// previous always-OR logic shipped with.
void test_quiet_window_contains_same_day_window() {
    QuietWindow q;
    q.loadFromStorage(true, 10, 0, 14, 0);
    TEST_ASSERT_TRUE(q.contains(10, 0));    // at start
    TEST_ASSERT_TRUE(q.contains(12, 30));   // middle
    TEST_ASSERT_TRUE(q.contains(13, 59));   // just before end
    TEST_ASSERT_FALSE(q.contains(14, 0));   // exactly at end
    TEST_ASSERT_FALSE(q.contains(20, 0));   // evening — outside
    TEST_ASSERT_FALSE(q.contains(8, 0));    // morning — outside
}

void test_quiet_window_contains_empty_window_never_matches() {
    QuietWindow q;
    q.loadFromStorage(true, 12, 0, 12, 0);  // start == end → empty
    TEST_ASSERT_FALSE(q.contains(12, 0));
    TEST_ASSERT_FALSE(q.contains(0, 0));
    TEST_ASSERT_FALSE(q.contains(23, 59));
}

// ── SleepTimeout ──────────────────────────────────────────────────────────

void test_sleep_timeout_default_is_5_min() {
    SleepTimeout s;
    TEST_ASSERT_EQUAL_INT(SleepTimeout::DEFAULT_MIN, s.minutes());
    TEST_ASSERT_TRUE(s.enabled());
}

void test_sleep_timeout_zero_disables() {
    SleepTimeout s;
    s.set(0);
    TEST_ASSERT_EQUAL_INT(0, s.minutes());
    TEST_ASSERT_FALSE(s.enabled());
}

void test_sleep_timeout_set_marks_dirty_only_on_change() {
    SleepTimeout s;
    s.set(SleepTimeout::DEFAULT_MIN);   // already at default
    TEST_ASSERT_FALSE(s.consumeDirty());
    s.set(10);
    TEST_ASSERT_EQUAL_INT(10, s.minutes());
    TEST_ASSERT_TRUE(s.consumeDirty());
    TEST_ASSERT_FALSE(s.consumeDirty()); // cleared on read
}

void test_sleep_timeout_clamps_below_zero() {
    SleepTimeout s;
    s.set(-5);
    TEST_ASSERT_EQUAL_INT(SleepTimeout::MIN_MIN, s.minutes());  // 0
}

void test_sleep_timeout_clamps_above_max() {
    SleepTimeout s;
    s.set(9999);
    TEST_ASSERT_EQUAL_INT(SleepTimeout::MAX_MIN, s.minutes());  // 60
}

void test_sleep_timeout_bump_up_and_down() {
    SleepTimeout s;
    s.set(10);
    s.consumeDirty();
    s.bumpUp();
    TEST_ASSERT_EQUAL_INT(11, s.minutes());
    s.bumpDown();
    s.bumpDown();
    TEST_ASSERT_EQUAL_INT(9, s.minutes());
}

void test_sleep_timeout_bump_down_past_zero_clamps() {
    SleepTimeout s;
    s.set(0);
    s.bumpDown();
    TEST_ASSERT_EQUAL_INT(0, s.minutes());  // can't go negative
    TEST_ASSERT_FALSE(s.enabled());
}

void test_sleep_timeout_bump_up_past_max_clamps() {
    SleepTimeout s;
    s.set(SleepTimeout::MAX_MIN);
    s.bumpUp();
    TEST_ASSERT_EQUAL_INT(SleepTimeout::MAX_MIN, s.minutes());
}

void test_sleep_timeout_load_from_storage_doesnt_mark_dirty() {
    SleepTimeout s;
    s.loadFromStorage(15);
    TEST_ASSERT_EQUAL_INT(15, s.minutes());
    TEST_ASSERT_FALSE(s.consumeDirty());
}

void test_sleep_timeout_load_from_storage_clamps_garbage() {
    SleepTimeout s;
    s.loadFromStorage(-100);   // corrupt NVS reading
    TEST_ASSERT_EQUAL_INT(0, s.minutes());
    s.loadFromStorage(1000);
    TEST_ASSERT_EQUAL_INT(SleepTimeout::MAX_MIN, s.minutes());
}

// ── WakeTime ──────────────────────────────────────────────────────────────

void test_wake_time_defaults() {
    WakeTime w;
    TEST_ASSERT_EQUAL_INT(WakeTime::DEFAULT_HOUR,   w.hour());
    TEST_ASSERT_EQUAL_INT(WakeTime::DEFAULT_MINUTE, w.minute());
}

void test_wake_time_set_marks_dirty_only_on_change() {
    WakeTime w;
    w.setHour(WakeTime::DEFAULT_HOUR);  // already there
    TEST_ASSERT_FALSE(w.consumeDirty());
    w.setHour(8);
    TEST_ASSERT_EQUAL_INT(8, w.hour());
    TEST_ASSERT_TRUE(w.consumeDirty());
    TEST_ASSERT_FALSE(w.consumeDirty());
}

void test_wake_time_hour_wraps() {
    WakeTime w;
    w.setHour(25);                     // wraps to 1
    TEST_ASSERT_EQUAL_INT(1, w.hour());
    w.setHour(-3);                     // wraps to 21
    TEST_ASSERT_EQUAL_INT(21, w.hour());
}

void test_wake_time_minute_snaps_to_step() {
    WakeTime w;
    w.setMinute(17);                   // not on a 5-min boundary
    TEST_ASSERT_EQUAL_INT(15, w.minute());  // snapped down
    w.setMinute(33);
    TEST_ASSERT_EQUAL_INT(30, w.minute());
    w.setMinute(60);                   // wraps to 0
    TEST_ASSERT_EQUAL_INT(0, w.minute());
}

void test_wake_time_bump_minute_moves_by_step() {
    WakeTime w;
    w.setMinute(30);
    w.consumeDirty();
    w.bumpMinute(+1);
    TEST_ASSERT_EQUAL_INT(35, w.minute());
    w.bumpMinute(-2);
    TEST_ASSERT_EQUAL_INT(25, w.minute());
}

void test_wake_time_bump_minute_wraps() {
    WakeTime w;
    w.setMinute(55);
    w.bumpMinute(+2);                  // 55 + 10 = 65 → 5
    TEST_ASSERT_EQUAL_INT(5, w.minute());
}

void test_wake_time_bump_hour_wraps_at_midnight() {
    WakeTime w;
    w.setHour(23);
    w.bumpHour(+1);
    TEST_ASSERT_EQUAL_INT(0, w.hour());
    w.bumpHour(-1);
    TEST_ASSERT_EQUAL_INT(23, w.hour());
}

void test_wake_time_load_from_storage_clears_dirty() {
    WakeTime w;
    w.loadFromStorage(7, 45);
    TEST_ASSERT_EQUAL_INT(7,  w.hour());
    TEST_ASSERT_EQUAL_INT(45, w.minute());
    TEST_ASSERT_FALSE(w.consumeDirty());
}

void test_wake_time_load_clamps_garbage() {
    WakeTime w;
    w.loadFromStorage(100, 999);
    // 100 % 24 = 4; 999 → snap (999/5*5=995) → 995 % 60 = 35
    TEST_ASSERT_EQUAL_INT(4,  w.hour());
    TEST_ASSERT_EQUAL_INT(35, w.minute());
}

// ── PortionState ──────────────────────────────────────────────────────────

void test_portion_state_default_is_DEFAULT_G() {
    PortionState p;
    TEST_ASSERT_EQUAL_INT(PortionState::DEFAULT_G, p.grams());
}

void test_portion_state_explicit_initial_is_clamped() {
    PortionState p(999);
    TEST_ASSERT_EQUAL_INT(PortionState::MAX_G, p.grams());
    PortionState q(-50);
    TEST_ASSERT_EQUAL_INT(PortionState::MIN_G, q.grams());
}

void test_portion_state_set_returns_true_on_change() {
    PortionState p;
    TEST_ASSERT_FALSE(p.set(PortionState::DEFAULT_G)); // unchanged
    TEST_ASSERT_TRUE(p.set(50));                       // changed
    TEST_ASSERT_EQUAL_INT(50, p.grams());
}

void test_portion_state_set_marks_dirty_only_on_change() {
    PortionState p;
    p.set(PortionState::DEFAULT_G);
    TEST_ASSERT_FALSE(p.consumeDirty());
    p.set(20);
    TEST_ASSERT_TRUE(p.consumeDirty());
    TEST_ASSERT_FALSE(p.consumeDirty());
}

void test_portion_state_clamps_below_min() {
    PortionState p;
    p.set(-10);
    TEST_ASSERT_EQUAL_INT(PortionState::MIN_G, p.grams());
}

void test_portion_state_clamps_above_max() {
    PortionState p;
    p.set(9999);
    TEST_ASSERT_EQUAL_INT(PortionState::MAX_G, p.grams());
}

void test_portion_state_bump_up_and_down_step() {
    PortionState p;
    p.set(40);
    p.consumeDirty();
    TEST_ASSERT_TRUE(p.bumpUp());
    TEST_ASSERT_EQUAL_INT(40 + PortionState::STEP_G, p.grams());
    TEST_ASSERT_TRUE(p.bumpDown());
    TEST_ASSERT_TRUE(p.bumpDown());
    TEST_ASSERT_EQUAL_INT(40 - PortionState::STEP_G, p.grams());
}

void test_portion_state_bump_at_max_no_change() {
    PortionState p;
    p.set(PortionState::MAX_G);
    p.consumeDirty();
    TEST_ASSERT_FALSE(p.bumpUp());                     // already at max
    TEST_ASSERT_FALSE(p.consumeDirty());
}

void test_portion_state_bump_at_min_no_change() {
    PortionState p;
    p.set(PortionState::MIN_G);
    p.consumeDirty();
    TEST_ASSERT_FALSE(p.bumpDown());                   // already at min
}

void test_portion_state_load_from_storage_clears_dirty() {
    PortionState p;
    p.loadFromStorage(35);
    TEST_ASSERT_EQUAL_INT(35, p.grams());
    TEST_ASSERT_FALSE(p.consumeDirty());
}

void test_portion_state_load_clamps_garbage() {
    PortionState p;
    p.loadFromStorage(-100);
    TEST_ASSERT_EQUAL_INT(PortionState::MIN_G, p.grams());
    p.loadFromStorage(500);
    TEST_ASSERT_EQUAL_INT(PortionState::MAX_G, p.grams());
}

// ── EventId ───────────────────────────────────────────────────────────────

void test_event_id_length_is_32_hex_chars() {
    std::string id = generateEventId();
    TEST_ASSERT_EQUAL_size_t(32, id.size());
}

void test_event_id_only_hex_chars() {
    std::string id = generateEventId();
    for (char c : id) {
        const bool isHex = (c >= '0' && c <= '9') ||
                           (c >= 'a' && c <= 'f');
        TEST_ASSERT_TRUE(isHex);
    }
}

void test_event_id_uniqueness_across_many_calls() {
    // Seed for determinism on host. Don't care about exact values —
    // just that 1000 draws don't collide (probability ~0).
    std::srand(42);
    std::set<std::string> seen;
    for (int i = 0; i < 1000; ++i) {
        seen.insert(generateEventId());
    }
    TEST_ASSERT_EQUAL_size_t(1000, seen.size());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mood_just_fed_overrides_everything);
    RUN_TEST(test_mood_snooze_overrides_age);
    RUN_TEST(test_mood_snooze_expired_falls_through);
    RUN_TEST(test_mood_never_fed_is_hungry);
    RUN_TEST(test_mood_happy_when_recently_fed);
    RUN_TEST(test_mood_neutral_in_second_quartile);
    RUN_TEST(test_mood_warning_in_third_quartile);
    RUN_TEST(test_mood_hungry_past_threshold);
    RUN_TEST(test_mood_clock_went_backwards_is_happy);
    RUN_TEST(test_ring_full_when_just_fed);
    RUN_TEST(test_ring_half_at_half_threshold);
    RUN_TEST(test_ring_empty_at_threshold);
    RUN_TEST(test_ring_empty_past_threshold);
    RUN_TEST(test_ring_zero_when_never_fed);
    RUN_TEST(test_ring_full_when_clock_backwards);
    RUN_TEST(test_ring_handles_zero_threshold_safely);

    RUN_TEST(test_tz_default_is_utc);
    RUN_TEST(test_tz_set_marks_dirty_and_clamps);
    RUN_TEST(test_tz_set_same_value_doesnt_mark_dirty);
    RUN_TEST(test_tz_clamps_below_min);
    RUN_TEST(test_tz_clamps_above_max);
    RUN_TEST(test_tz_bump_hour_positive);
    RUN_TEST(test_tz_bump_hour_negative_past_min_clamps);
    RUN_TEST(test_tz_load_from_storage_doesnt_mark_dirty);

    RUN_TEST(test_cat_roster_seed_default_creates_one_cat);
    RUN_TEST(test_cat_roster_seed_default_idempotent);
    RUN_TEST(test_cat_roster_add_assigns_climbing_ids);
    RUN_TEST(test_cat_roster_add_caps_at_max);
    RUN_TEST(test_cat_roster_remove_refuses_last);
    RUN_TEST(test_cat_roster_remove_shifts_indices);
    RUN_TEST(test_cat_roster_remove_preserves_next_id);
    RUN_TEST(test_cat_roster_remove_clamps_active_idx);
    RUN_TEST(test_cat_roster_remove_resets_feed_selection_when_out_of_range);
    RUN_TEST(test_cat_roster_remove_marks_dirty);
    RUN_TEST(test_cat_roster_find_slot_by_id);
    RUN_TEST(test_cat_roster_feed_selection_rejects_out_of_range);
    RUN_TEST(test_cat_roster_append_loaded_seeds_next_id);
    RUN_TEST(test_cat_roster_active_threshold_dirty_only_on_change);

    RUN_TEST(test_user_roster_primary_name_falls_back_to_you);
    RUN_TEST(test_user_roster_primary_name_uses_first_user);
    RUN_TEST(test_user_roster_current_feeder_name_falls_back_to_primary);
    RUN_TEST(test_user_roster_current_feeder_name_uses_picker);
    RUN_TEST(test_user_roster_clear_current_feeder_reverts_to_primary);
    RUN_TEST(test_user_roster_set_current_feeder_rejects_out_of_range);
    RUN_TEST(test_user_roster_picker_is_not_dirty_tracked);

    RUN_TEST(test_meal_schedule_current_slot_morning);
    RUN_TEST(test_meal_schedule_current_slot_afternoon);
    RUN_TEST(test_meal_schedule_current_slot_wraps_post_treat);
    RUN_TEST(test_meal_schedule_set_slot_hour_wraps);
    RUN_TEST(test_meal_schedule_set_returns_false_when_unchanged);
    RUN_TEST(test_meal_schedule_is_served_strict);
    RUN_TEST(test_meal_schedule_invalid_slot_idx);

    RUN_TEST(test_quiet_window_defaults_disabled);
    RUN_TEST(test_quiet_window_contains_during_evening);
    RUN_TEST(test_quiet_window_contains_during_overnight);
    RUN_TEST(test_quiet_window_contains_excludes_daytime);
    RUN_TEST(test_quiet_window_minute_step_snaps);
    RUN_TEST(test_quiet_window_toggle_marks_dirty);
    RUN_TEST(test_quiet_window_load_clears_dirty);
    RUN_TEST(test_quiet_window_bump_hour_wraps);
    RUN_TEST(test_quiet_window_contains_same_day_window);
    RUN_TEST(test_quiet_window_contains_empty_window_never_matches);

    RUN_TEST(test_sleep_timeout_default_is_5_min);
    RUN_TEST(test_sleep_timeout_zero_disables);
    RUN_TEST(test_sleep_timeout_set_marks_dirty_only_on_change);
    RUN_TEST(test_sleep_timeout_clamps_below_zero);
    RUN_TEST(test_sleep_timeout_clamps_above_max);
    RUN_TEST(test_sleep_timeout_bump_up_and_down);
    RUN_TEST(test_sleep_timeout_bump_down_past_zero_clamps);
    RUN_TEST(test_sleep_timeout_bump_up_past_max_clamps);
    RUN_TEST(test_sleep_timeout_load_from_storage_doesnt_mark_dirty);
    RUN_TEST(test_sleep_timeout_load_from_storage_clamps_garbage);

    RUN_TEST(test_wake_time_defaults);
    RUN_TEST(test_wake_time_set_marks_dirty_only_on_change);
    RUN_TEST(test_wake_time_hour_wraps);
    RUN_TEST(test_wake_time_minute_snaps_to_step);
    RUN_TEST(test_wake_time_bump_minute_moves_by_step);
    RUN_TEST(test_wake_time_bump_minute_wraps);
    RUN_TEST(test_wake_time_bump_hour_wraps_at_midnight);
    RUN_TEST(test_wake_time_load_from_storage_clears_dirty);
    RUN_TEST(test_wake_time_load_clamps_garbage);

    RUN_TEST(test_portion_state_default_is_DEFAULT_G);
    RUN_TEST(test_portion_state_explicit_initial_is_clamped);
    RUN_TEST(test_portion_state_set_returns_true_on_change);
    RUN_TEST(test_portion_state_set_marks_dirty_only_on_change);
    RUN_TEST(test_portion_state_clamps_below_min);
    RUN_TEST(test_portion_state_clamps_above_max);
    RUN_TEST(test_portion_state_bump_up_and_down_step);
    RUN_TEST(test_portion_state_bump_at_max_no_change);
    RUN_TEST(test_portion_state_bump_at_min_no_change);
    RUN_TEST(test_portion_state_load_from_storage_clears_dirty);
    RUN_TEST(test_portion_state_load_clamps_garbage);

    RUN_TEST(test_event_id_length_is_32_hex_chars);
    RUN_TEST(test_event_id_only_hex_chars);
    RUN_TEST(test_event_id_uniqueness_across_many_calls);
    return UNITY_END();
}
