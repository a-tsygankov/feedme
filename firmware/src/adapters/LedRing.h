#pragma once

#include <Adafruit_NeoPixel.h>
#include <stdint.h>

namespace feedme::adapters {

// 5-LED WS2812 ring under the CrowPanel bezel (data on GPIO 48).
// Used as quick visual feedback for tap/long-press events. Idle = off.
//
// API:
//   begin()             - initialise the underlying driver
//   pulse(color, ms)    - light all LEDs solid `color` for `ms` then off
//   tick()              - advance pulse animation; call from loop()
class LedRing {
public:
    static constexpr int      DATA_PIN  = 48;
    static constexpr uint16_t LED_COUNT = 5;

    void begin();
    void pulse(uint32_t color, uint16_t durationMs = 600);
    void tick();
    void off();

private:
    Adafruit_NeoPixel pixels_{LED_COUNT, DATA_PIN, NEO_GRB + NEO_KHZ800};
    uint32_t pulseEndMs_ = 0;
    bool     lit_        = false;
};

}  // namespace feedme::adapters
