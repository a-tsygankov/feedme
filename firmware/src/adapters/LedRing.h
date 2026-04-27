#pragma once

#include <Adafruit_NeoPixel.h>
#include <stdint.h>

namespace feedme::adapters {

// 5-LED WS2812 ring under the CrowPanel bezel (data on GPIO 48).
//
// Two visual layers:
//   - **Ambient**: steady, dim breathing in a mood colour. Set with
//     setAmbient(color); pass 0 to turn off.
//   - **Pulse**: short bright burst over the ambient layer for taps,
//     snooze, history. Reverts to ambient after `durationMs`.
//
// tick() should be called from loop() to drive the breathing animation.
class LedRing {
public:
    static constexpr int      DATA_PIN  = 48;
    static constexpr uint16_t LED_COUNT = 5;

    void begin();
    void pulse(uint32_t color, uint16_t durationMs = 600);
    void setAmbient(uint32_t color);   // 0 = off
    void tick();
    void off();

private:
    void renderAmbientFrame();

    Adafruit_NeoPixel pixels_{LED_COUNT, DATA_PIN, NEO_GRB + NEO_KHZ800};
    uint32_t pulseEndMs_   = 0;
    bool     pulseActive_  = false;
    uint32_t ambientColor_ = 0;
    uint32_t lastAnimMs_   = 0;
};

}  // namespace feedme::adapters
