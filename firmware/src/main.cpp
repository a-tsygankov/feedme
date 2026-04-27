// Composition root. The only file that knows about concrete adapters.
//
// On the simulator (-DSIMULATOR), wires a SimulatedClock that runs ~720x real
// time so the four mood states cycle through in about a minute of wall clock.

#include <Arduino.h>

#include "adapters/ArduinoClock.h"
#include "adapters/LvglDisplay.h"
#include "adapters/NoopNetwork.h"
#include "adapters/NoopStorage.h"
#include "adapters/SimulatedClock.h"
#include "application/DisplayCoordinator.h"
#include "application/FeedingService.h"

#if defined(SIMULATOR)
#  include "adapters/StubTapSensor.h"
#else
#  include "adapters/Cst816TapSensor.h"
#  include "adapters/EncoderButtonSensor.h"
#endif

namespace {

// 5 hours per the design.
constexpr int64_t HUNGRY_THRESHOLD_SEC = 5 * 3600;

#if defined(SIMULATOR)
// Pretend "now" started at a fixed epoch on Apr 26 2026 12:00 UTC.
// Fast-forward factor of 720 = 12 minutes per real second; full Happy→Hungry
// arc takes ~25s of wall clock.
feedme::adapters::SimulatedClock simClock(1745668800LL, 720);
feedme::ports::IClock& clock = simClock;
#else
feedme::adapters::ArduinoClock realClock;
feedme::ports::IClock& clock = realClock;
#endif

feedme::adapters::LvglDisplay display;
feedme::adapters::NoopNetwork network;
feedme::adapters::NoopStorage storage;
#if defined(SIMULATOR)
feedme::adapters::StubTapSensor       taps;
feedme::adapters::StubTapSensor       button;
#else
feedme::adapters::Cst816TapSensor     taps;     // capacitive screen
feedme::adapters::EncoderButtonSensor button;   // physical knob press
#endif

constexpr int SNOOZE_DURATION_SEC = 30 * 60;  // long-press = 30 minutes

feedme::application::FeedingService feeding(clock, network, storage);
feedme::application::DisplayCoordinator displayCoord(
    display, feeding, clock, HUNGRY_THRESHOLD_SEC);

uint32_t lastServiceTickMs = 0;

}  // namespace

void setup() {
    // ── Power up the LCD rail BEFORE Serial or anything else. ────────────
    // The CrowPanel 1.28" Rotary Display gates the LCD's 3V3 supply
    // through a load switch controlled by GPIO 1; without driving it
    // HIGH the LCD chip is completely unpowered and no SPI command,
    // backlight pulse, or reset will ever produce a visible result.
    // GPIO 2 is a similar gate for the LED-ring 3V3 rail.
    pinMode(1, OUTPUT);
    digitalWrite(1, HIGH);
    pinMode(2, OUTPUT);
    digitalWrite(2, HIGH);

    Serial.begin(115200);
    const uint32_t serialDeadline = millis() + 3000;
    while (!Serial && millis() < serialDeadline) {
        delay(10);
    }
    Serial.println("[feedme] boot");

    display.begin();
    Serial.println("[feedme] display ready");

    network.begin();
    storage.begin();
    taps.begin();
    button.begin();

    // Two input devices, one shared handler. Capacitive screen taps
    // and physical knob presses both end up here.
    //
    //   Tap        (capacitive)   -> log feed
    //   DoubleTap  (capacitive)   -> view history (TODO)
    //   Press      (physical)     -> log feed (alternative tactile path)
    //   LongPress  (physical)     -> snooze 30 min
    auto handleInput = [](feedme::ports::TapEvent ev) {
        using E = feedme::ports::TapEvent;
        switch (ev) {
            case E::Tap:
                Serial.println("[input] tap -> log feed");
                feeding.logFeeding("user");
                break;
            case E::Press:
                Serial.println("[input] press -> log feed");
                feeding.logFeeding("user");
                break;
            case E::DoubleTap:
                Serial.println("[input] double-tap -> history (TODO)");
                break;
            case E::LongPress:
                Serial.println("[input] long-press -> snooze 30m");
                feeding.snooze("user", SNOOZE_DURATION_SEC);
                break;
        }
    };
    taps.onEvent(handleInput);
    button.onEvent(handleInput);

    Serial.println("[feedme] setup complete");

#if defined(SIMULATOR)
    // Pretend the cat was fed an hour ago at boot, then never again, so we
    // start in Happy and walk the full arc as time accelerates.
    feeding.logFeeding("sim");
#endif
}

void loop() {
    const uint32_t now = millis();

    // Heartbeat for the first ~5 s so we can confirm loop() is ticking.
    static uint32_t lastBeatMs = 0;
    static uint32_t beatCount = 0;
    if (beatCount < 5 && now - lastBeatMs >= 1000) {
        lastBeatMs = now;
        Serial.print("[feedme] loop beat ");
        Serial.println(beatCount++);
    }

    taps.poll();
    button.poll();

    if (now - lastServiceTickMs >= 1000) {
        lastServiceTickMs = now;
        feeding.tick();
    }

    displayCoord.tick();
    delay(5);
}
