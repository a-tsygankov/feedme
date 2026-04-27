#pragma once

#include <functional>

namespace feedme::ports {

// Two distinct input paths feed into a single event stream:
//
//   Capacitive screen (CST816D-style touch IC):
//     Tap        — finger lightly taps the LCD glass
//     DoubleTap  — two quick taps within ~300 ms
//
//   Physical knob/screen press (rotary-encoder push button):
//     Press      — full tactile click of the whole knob
//     LongPress  — knob held down longer than ~600 ms
//
// Adapters fire whichever events match their input device. The
// composition root maps them to actions.
enum class TapEvent {
    Tap,        // capacitive: light single tap on the screen surface
    DoubleTap,  // capacitive: two taps within the double-tap window
    Press,      // physical: tactile click of the whole knob/screen
    LongPress,  // physical: knob held past the long-press threshold
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
