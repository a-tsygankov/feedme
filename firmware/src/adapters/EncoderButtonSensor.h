#pragma once

#include "ports/ITapSensor.h"

#include <stdint.h>

namespace feedme::adapters {

// All three CrowPanel rotary-encoder inputs in one adapter:
//   - Push switch on GPIO 41 (active-LOW, internal pull-up)
//       -> Press, DoublePress, LongPress
//   - Quadrature channels on GPIO 45 (A) and GPIO 42 (B)
//       -> RotateCW, RotateCCW
//
// The class name is kept as EncoderButtonSensor for back-compat;
// it covers the whole knob now (rotation + click).
class EncoderButtonSensor : public feedme::ports::ITapSensor {
public:
    void begin() override;
    void onEvent(Listener listener) override;
    void poll() override;

private:
    void emit(feedme::ports::TapEvent ev);

    Listener listener_;

    // ── Push-switch state ────────────────────────────────────────────
    bool     wasPressed_      = false;
    bool     longPressFired_  = false;
    bool     pendingPress_    = false;   // waiting to see if a 2nd click follows
    uint32_t pressStartMs_    = 0;
    uint32_t lastReleaseMs_   = 0;
    uint32_t lastEdgeMs_      = 0;       // debounce

    // ── Quadrature decoder state ─────────────────────────────────────
    uint8_t  lastAB_          = 0;
    int8_t   subStep_         = 0;       // accumulator: ±4 = one detent
};

}  // namespace feedme::adapters
