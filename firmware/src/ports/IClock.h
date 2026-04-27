#pragma once

#include <cstdint>

namespace feedme::ports {

// Abstraction over "what time is it now?" — lets the application layer
// remain testable and lets the simulator run a fast-forwarded clock.
class IClock {
public:
    virtual ~IClock() = default;
    // Unix seconds since epoch, monotonic-ish; may go forward fast in sim.
    virtual int64_t nowSec() = 0;
};

}  // namespace feedme::ports
