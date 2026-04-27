#pragma once

#include "ports/IClock.h"

namespace feedme::adapters {

// Runs the clock at an accelerated rate so the simulator can show all four
// mood states in seconds rather than hours. Uses millis() under the hood.
class SimulatedClock : public feedme::ports::IClock {
public:
    explicit SimulatedClock(int64_t startEpochSec, int speedupFactor)
        : start_(startEpochSec), speedup_(speedupFactor) {}

    int64_t nowSec() override;

private:
    int64_t start_;
    int     speedup_;
};

}  // namespace feedme::adapters
