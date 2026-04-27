#pragma once

#include "ports/ITapSensor.h"

#include <stdint.h>

namespace feedme::adapters {

// Physical-press input via the CrowPanel rotary encoder's push switch
// on GPIO 41 (active-LOW, internal pull-up). Wholly distinct from the
// CST816D capacitive touch; this fires when the user pushes the entire
// knob/screen assembly in like a button.
//
// Emits:
//   Press      — short tactile click (released within ~600 ms)
//   LongPress  — held longer than ~600 ms (used for snooze)
class EncoderButtonSensor : public feedme::ports::ITapSensor {
public:
    void begin() override;
    void onEvent(Listener listener) override;
    void poll() override;

private:
    void emit(feedme::ports::TapEvent ev);

    Listener listener_;
    bool     wasPressed_      = false;
    bool     longPressFired_  = false;
    uint32_t pressStartMs_    = 0;
    uint32_t lastEdgeMs_      = 0;  // for debounce
};

}  // namespace feedme::adapters
