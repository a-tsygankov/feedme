#pragma once

#include "ports/ITapSensor.h"

namespace feedme::adapters {

// Headless tap sensor for the simulator: never fires. Lets the rest of the
// stack run unchanged. Real hardware uses a Qmi8658TapSensor adapter (TODO).
class StubTapSensor : public feedme::ports::ITapSensor {
public:
    void begin() override {}
    void onEvent(Listener) override {}
    void poll() override {}
};

}  // namespace feedme::adapters
