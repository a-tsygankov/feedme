#pragma once

#include "domain/FeedingState.h"
#include <cstdint>

namespace feedme::domain {

// Pure function. Returns the fraction of the ring that should be filled,
// in [0.0, 1.0]. 1.0 = freshly fed, 0.0 = at-or-past the hungry threshold.
float computeRingProgress(const FeedingState& state,
                          int64_t nowTs,
                          int64_t hungryThresholdSec);

}  // namespace feedme::domain
