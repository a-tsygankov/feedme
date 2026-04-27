#include "adapters/ArduinoClock.h"

#include <Arduino.h>
#include <ctime>

namespace feedme::adapters {

int64_t ArduinoClock::nowSec() {
    const time_t t = time(nullptr);
    if (t > 1577836800) {  // 2020-01-01: any later value implies NTP succeeded
        return static_cast<int64_t>(t);
    }
    return static_cast<int64_t>(millis() / 1000);
}

}  // namespace feedme::adapters
