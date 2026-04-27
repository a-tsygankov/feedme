#include "adapters/EncoderButtonSensor.h"

#include <Arduino.h>

namespace feedme::adapters {

namespace {

constexpr int      PIN_SWITCH      = 41;   // rotary-encoder push switch
constexpr int      PIN_A           = 45;   // quadrature channel A
constexpr int      PIN_B           = 42;   // quadrature channel B
constexpr uint32_t DEBOUNCE_MS     = 20;
constexpr uint32_t LONG_PRESS_MS   = 600;  // hold past this -> LongPress
constexpr uint32_t DOUBLE_PRESS_MS = 350;  // two clicks within this -> DoublePress

// Quadrature decode lookup. Index = (prev_AB << 2) | curr_AB.
// Valid CW transitions count +1, valid CCW count -1, rest are
// invalid (bounce / double-step) and count 0.
constexpr int8_t QUAD_LUT[16] = {
    0, -1, +1,  0,
   +1,  0,  0, -1,
   -1,  0,  0, +1,
    0, +1, -1,  0,
};

}  // namespace

void EncoderButtonSensor::begin() {
    pinMode(PIN_SWITCH, INPUT_PULLUP);
    pinMode(PIN_A,      INPUT_PULLUP);
    pinMode(PIN_B,      INPUT_PULLUP);
    lastAB_ = static_cast<uint8_t>(
        (digitalRead(PIN_A) << 1) | digitalRead(PIN_B));
    Serial.println("[knob] ready (switch GPIO 41, A=45, B=42)");
}

void EncoderButtonSensor::onEvent(Listener listener) {
    listener_ = listener;
}

void EncoderButtonSensor::poll() {
    const uint32_t now = millis();

    // ── Quadrature rotation ──────────────────────────────────────────
    const uint8_t curr = static_cast<uint8_t>(
        (digitalRead(PIN_A) << 1) | digitalRead(PIN_B));
    if (curr != lastAB_) {
        const uint8_t idx = static_cast<uint8_t>((lastAB_ << 2) | curr);
        subStep_ = static_cast<int8_t>(subStep_ + QUAD_LUT[idx]);
        lastAB_  = curr;
        if (subStep_ >= 4) {
            subStep_ = 0;
            emit(feedme::ports::TapEvent::RotateCW);
        } else if (subStep_ <= -4) {
            subStep_ = 0;
            emit(feedme::ports::TapEvent::RotateCCW);
        }
    }

    // ── Push switch ──────────────────────────────────────────────────
    const bool pressed = digitalRead(PIN_SWITCH) == LOW;

    if (pressed != wasPressed_) {
        // Edge: apply software debounce.
        if (now - lastEdgeMs_ < DEBOUNCE_MS) return;
        lastEdgeMs_ = now;

        if (pressed) {
            // Press start.
            wasPressed_     = true;
            pressStartMs_   = now;
            longPressFired_ = false;
        } else {
            // Press end. Skip Press if LongPress already fired.
            wasPressed_ = false;
            if (longPressFired_) {
                pendingPress_ = false;
            } else if (pendingPress_ &&
                       (now - lastReleaseMs_) < DOUBLE_PRESS_MS) {
                emit(feedme::ports::TapEvent::DoublePress);
                pendingPress_ = false;
            } else {
                pendingPress_  = true;
                lastReleaseMs_ = now;
            }
        }
        return;
    }

    // No edge: check for long-press threshold and pending-press timeout.
    if (wasPressed_ && !longPressFired_ &&
        (now - pressStartMs_) >= LONG_PRESS_MS) {
        longPressFired_ = true;
        emit(feedme::ports::TapEvent::LongPress);
    }

    if (pendingPress_ && !wasPressed_ &&
        (now - lastReleaseMs_) >= DOUBLE_PRESS_MS) {
        emit(feedme::ports::TapEvent::Press);
        pendingPress_ = false;
    }
}

void EncoderButtonSensor::emit(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    const char* name =
        ev == E::Press       ? "press"        :
        ev == E::DoublePress ? "double-press" :
        ev == E::LongPress   ? "long-press"   :
        ev == E::RotateCW    ? "rotate-cw"    :
        ev == E::RotateCCW   ? "rotate-ccw"   :
                               "?";
    Serial.printf("[knob] %s\n", name);
    if (listener_) listener_(ev);
}

}  // namespace feedme::adapters
