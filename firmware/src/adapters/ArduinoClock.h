#pragma once

#include "ports/IClock.h"

namespace feedme::adapters {

// Real-time clock for production. Uses time(nullptr) once NTP has set the
// system clock; falls back to monotonic millis() / 1000 before that.
class ArduinoClock : public feedme::ports::IClock {
public:
    int64_t nowSec() override;
};

}  // namespace feedme::adapters
