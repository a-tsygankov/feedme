#include "adapters/EncoderButtonSensor.h"

#include <Arduino.h>

namespace feedme::adapters {

namespace {

constexpr int      PIN_SWITCH      = 41;   // rotary-encoder push switch
constexpr uint32_t DEBOUNCE_MS     = 20;
constexpr uint32_t LONG_PRESS_MS   = 600;  // hold past this -> LongPress

}  // namespace

void EncoderButtonSensor::begin() {
    pinMode(PIN_SWITCH, INPUT_PULLUP);
    Serial.println("[btn] ready (encoder switch on GPIO 41)");
}

void EncoderButtonSensor::onEvent(Listener listener) {
    listener_ = listener;
}

void EncoderButtonSensor::poll() {
    const uint32_t now = millis();
    // Active-LOW: pin reads LOW when the knob is pressed.
    const bool pressed = digitalRead(PIN_SWITCH) == LOW;

    if (pressed != wasPressed_) {
        // Edge detected — apply software debounce.
        if (now - lastEdgeMs_ < DEBOUNCE_MS) return;
        lastEdgeMs_ = now;

        if (pressed) {
            // Press start.
            wasPressed_     = true;
            pressStartMs_   = now;
            longPressFired_ = false;
        } else {
            // Press end. If we already fired LongPress mid-hold,
            // don't also fire Press on release.
            wasPressed_ = false;
            if (!longPressFired_) {
                emit(feedme::ports::TapEvent::Press);
            }
        }
        return;
    }

    // No edge — check if a held press has crossed the long-press
    // threshold and fire LongPress eagerly (don't wait for release).
    if (wasPressed_ && !longPressFired_ &&
        (now - pressStartMs_) >= LONG_PRESS_MS) {
        longPressFired_ = true;
        emit(feedme::ports::TapEvent::LongPress);
    }
}

void EncoderButtonSensor::emit(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    const char* name = ev == E::Press ? "press" : "long-press";
    Serial.printf("[btn] %s\n", name);
    if (listener_) listener_(ev);
}

}  // namespace feedme::adapters
