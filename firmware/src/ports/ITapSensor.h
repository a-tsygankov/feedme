#pragma once

#include <functional>

namespace feedme::ports {

// Three distinct input channels feed into a single event stream:
//
//   Capacitive screen (CST816D-style touch IC) — light finger contact:
//     Tap         — quick touch and release
//     DoubleTap   — two quick taps within ~300 ms
//     LongTouch   — finger held on the glass past ~600 ms
//
//   Physical knob press (rotary-encoder push switch) — tactile click:
//     Press       — full click and release
//     DoublePress — two clicks within ~350 ms
//     LongPress   — knob held down past ~600 ms
//
//   Rotary encoder rotation:
//     RotateCW    — clockwise detent
//     RotateCCW   — counter-clockwise detent
//
// Adapters fire whichever events match their input device. The
// composition root maps each to an action.
enum class TapEvent {
    Tap,
    DoubleTap,
    LongTouch,
    Press,
    DoublePress,
    LongPress,
    RotateCW,
    RotateCCW,
};

class ITapSensor {
public:
    using Listener = std::function<void(TapEvent)>;

    virtual ~ITapSensor() = default;
    virtual void begin() = 0;
    virtual void onEvent(Listener listener) = 0;
    virtual void poll() = 0;  // call from loop()
};

}  // namespace feedme::ports
