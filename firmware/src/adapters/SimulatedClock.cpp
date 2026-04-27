#include "adapters/SimulatedClock.h"

#include <Arduino.h>

namespace feedme::adapters {

int64_t SimulatedClock::nowSec() {
    return start_ + static_cast<int64_t>(millis() / 1000) * speedup_;
}

}  // namespace feedme::adapters
