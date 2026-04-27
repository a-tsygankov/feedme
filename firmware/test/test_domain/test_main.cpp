#include <unity.h>

#include "domain/FeedingState.h"
#include "domain/Mood.h"
#include "domain/MoodCalculator.h"
#include "domain/RingProgress.h"

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
    return UNITY_END();
}
