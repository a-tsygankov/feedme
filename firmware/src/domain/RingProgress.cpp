#include "domain/RingProgress.h"

namespace feedme::domain {

float computeRingProgress(const FeedingState& state,
                          int64_t nowTs,
                          int64_t hungryThresholdSec) {
    if (hungryThresholdSec <= 0) {
        return 0.0f;
    }
    if (state.lastFeedTs == 0) {
        return 0.0f;
    }
    const int64_t age = nowTs - state.lastFeedTs;
    if (age <= 0) {
        return 1.0f;
    }
    if (age >= hungryThresholdSec) {
        return 0.0f;
    }
    return 1.0f - static_cast<float>(age) / static_cast<float>(hungryThresholdSec);
}

}  // namespace feedme::domain
