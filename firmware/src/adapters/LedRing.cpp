#include "adapters/LedRing.h"

#include <Arduino.h>

namespace feedme::adapters {

void LedRing::begin() {
    pixels_.begin();
    pixels_.setBrightness(128);  // ~50% — bright enough to see, easy on eyes
    off();
    Serial.println("[led] ready (WS2812 x5 on GPIO 48)");
}

void LedRing::pulse(uint32_t color, uint16_t durationMs) {
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        pixels_.setPixelColor(i, color);
    }
    pixels_.show();
    pulseEndMs_ = millis() + durationMs;
    lit_ = true;
}

void LedRing::tick() {
    if (lit_ && static_cast<int32_t>(millis() - pulseEndMs_) >= 0) {
        off();
    }
}

void LedRing::off() {
    for (uint16_t i = 0; i < LED_COUNT; ++i) {
        pixels_.setPixelColor(i, 0);
    }
    pixels_.show();
    lit_ = false;
}

}  // namespace feedme::adapters
