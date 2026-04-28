#pragma once

#include <cstdint>

namespace feedme::ports {

// Tiny key/value port for user preferences that should survive reboots.
// Backed by NVS on the real board, no-op in the simulator. Each adapter
// is responsible for its own namespace; the keys here are stable strings.
class IPreferences {
public:
    virtual ~IPreferences() = default;
    virtual void  begin() = 0;

    // hungryThresholdSec — the value DisplayCoordinator clamps and uses.
    virtual int64_t getHungryThresholdSec(int64_t defaultValue) = 0;
    virtual void    setHungryThresholdSec(int64_t value) = 0;
};

}  // namespace feedme::ports
