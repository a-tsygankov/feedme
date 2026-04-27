#include "adapters/Cst816TapSensor.h"

#include <Arduino.h>
#include <Wire.h>

namespace feedme::adapters {

namespace {

constexpr uint8_t I2C_ADDR        = 0x15;
constexpr uint8_t REG_FINGER_NUM  = 0x02;

// Pin map (CrowPanel 1.28-inch HMI, per Elecrow wiki).
constexpr int PIN_SDA = 6;
constexpr int PIN_SCL = 7;
constexpr int PIN_RST = 13;
constexpr int PIN_INT = 5;

// Gesture timing thresholds (milliseconds).
constexpr uint32_t LONG_TOUCH_MS    = 600;  // touches held longer than this
                                            // are ignored (use the physical
                                            // button for long-press / snooze)
constexpr uint32_t DOUBLE_TAP_MS    = 300;  // two taps within this -> DoubleTap
constexpr uint32_t POLL_INTERVAL_MS = 20;   // I²C read cadence; ~50 Hz

uint8_t readReg(uint8_t reg) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0;
    if (Wire.requestFrom(static_cast<uint8_t>(I2C_ADDR),
                         static_cast<uint8_t>(1)) != 1) return 0;
    return Wire.read();
}

}  // namespace

void Cst816TapSensor::begin() {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, LOW);
    delay(20);
    digitalWrite(PIN_RST, HIGH);
    delay(50);

    Wire.setPins(PIN_SDA, PIN_SCL);
    Wire.begin();
    Wire.setClock(400000);

    pinMode(PIN_INT, INPUT_PULLUP);

    Serial.println("[cst816] ready (timing-based gesture detector)");
}

void Cst816TapSensor::onEvent(Listener listener) {
    listener_ = listener;
}

void Cst816TapSensor::poll() {
    // The chip's gesture register on this CrowPanel variant always
    // reports 0x0C regardless of touch duration, so we ignore the
    // gesture engine entirely and synthesize SingleTap/DoubleTap/
    // LongPress from raw finger-down timing.

    const uint32_t now = millis();
    if (now - lastPollMs_ < POLL_INTERVAL_MS) return;
    lastPollMs_ = now;

    // Read finger count (0 = no touch, 1+ = touch active).
    const bool touching = readReg(REG_FINGER_NUM) > 0;

    if (touching && !wasTouching_) {
        // Touch just started.
        wasTouching_  = true;
        touchStartMs_ = now;
    } else if (!touching && wasTouching_) {
        // Touch just ended.
        wasTouching_ = false;
        const uint32_t duration = now - touchStartMs_;

        if (duration >= LONG_TOUCH_MS) {
            // Long capacitive holds are ignored — those should come
            // through the physical knob press instead.
            pendingTap_ = false;
        } else if (pendingTap_ && (now - lastTapEndMs_) < DOUBLE_TAP_MS) {
            // Second short tap inside the double-tap window.
            emit(feedme::ports::TapEvent::DoubleTap);
            pendingTap_ = false;
        } else {
            // Short tap — wait briefly to see if a second one arrives.
            pendingTap_   = true;
            lastTapEndMs_ = now;
        }
    }

    // Pending single tap times out without a follow-up -> emit Tap.
    if (pendingTap_ && !wasTouching_ &&
        (now - lastTapEndMs_) >= DOUBLE_TAP_MS) {
        emit(feedme::ports::TapEvent::Tap);
        pendingTap_ = false;
    }
}

void Cst816TapSensor::emit(feedme::ports::TapEvent ev) {
    using E = feedme::ports::TapEvent;
    const char* name = ev == E::Tap ? "tap" : "double-tap";
    Serial.printf("[cst816] %s\n", name);
    if (listener_) listener_(ev);
}

}  // namespace feedme::adapters
