#pragma once

#include "domain/FeedingState.h"
#include "domain/Mood.h"
#include <cstdint>

namespace feedme::domain {

// Pure function. Given the state, the current time, and the threshold
// (seconds beyond which the cat is "fully hungry"), return the mood.
//
// Order of precedence:
//   1. justFed   → Fed
//   2. snoozed   → Sleepy
//   3. age-based → Happy / Neutral / Warning / Hungry
Mood calculateMood(const FeedingState& state,
                   int64_t nowTs,
                   int64_t hungryThresholdSec);

}  // namespace feedme::domain
