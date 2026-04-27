#include "domain/MoodCalculator.h"

namespace feedme::domain {

Mood calculateMood(const FeedingState& state,
                   int64_t nowTs,
                   int64_t hungryThresholdSec) {
    if (state.justFed) {
        return Mood::Fed;
    }
    if (state.snoozeUntilTs > nowTs) {
        return Mood::Sleepy;
    }

    // Never fed → treat as fully hungry.
    if (state.lastFeedTs == 0) {
        return Mood::Hungry;
    }

    const int64_t age = nowTs - state.lastFeedTs;
    if (age < 0) {
        // Clock went backwards — be charitable.
        return Mood::Happy;
    }

    // Quartile-based bucketing of the threshold window.
    if (age < hungryThresholdSec / 2) {
        return Mood::Happy;
    }
    if (age < (hungryThresholdSec * 3) / 4) {
        return Mood::Neutral;
    }
    if (age < hungryThresholdSec) {
        return Mood::Warning;
    }
    return Mood::Hungry;
}

}  // namespace feedme::domain
