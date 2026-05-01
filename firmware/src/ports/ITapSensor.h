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

    // Continuous press state — true while the physical input is engaged
    // (button down / finger on glass). LockConfirmView polls this each
    // render to detect release during the hold-to-confirm window, since
    // the discrete TapEvent stream only fires LongPress/LongTouch once
    // at the 600 ms threshold and gives no release signal afterwards.
    // Default is false so adapters that don't need this stay correct.
    virtual bool isPressed() const { return false; }

    // Last-touch coordinates in screen pixels (0..239 for both axes
    // on the 240×240 round LCD), captured at touch-end. Available for
    // touch sensors that decode position; defaults to -1 / -1 for
    // sensors without coords (the encoder button) so callers can
    // sniff for "did we get a real position" with `>= 0`. Views that
    // care (e.g. MenuView for tap-on-glyph) query this in handleInput
    // when a Tap event arrives.
    virtual int lastTouchX() const { return -1; }
    virtual int lastTouchY() const { return -1; }
};

}  // namespace feedme::ports
