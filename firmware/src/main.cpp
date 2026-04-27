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
#include "adapters/StubTapSensor.h"
#include "application/DisplayCoordinator.h"
#include "application/FeedingService.h"

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

feedme::adapters::LvglDisplay   display;
feedme::adapters::NoopNetwork   network;
feedme::adapters::NoopStorage   storage;
feedme::adapters::StubTapSensor taps;

feedme::application::FeedingService feeding(clock, network, storage);
feedme::application::DisplayCoordinator displayCoord(
    display, feeding, clock, HUNGRY_THRESHOLD_SEC);

uint32_t lastServiceTickMs = 0;

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("[feedme] boot");

    display.begin();
    network.begin();
    storage.begin();
    taps.begin();

#if defined(SIMULATOR)
    // Pretend the cat was fed an hour ago at boot, then never again, so we
    // start in Happy and walk the full arc as time accelerates.
    feeding.logFeeding("sim");
#endif
}

void loop() {
    const uint32_t now = millis();

    taps.poll();

    if (now - lastServiceTickMs >= 1000) {
        lastServiceTickMs = now;
        feeding.tick();
    }

    displayCoord.tick();
    delay(5);
}
