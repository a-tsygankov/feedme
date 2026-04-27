// Composition root. The only file that knows about concrete adapters.
//
// On the simulator (-DSIMULATOR), wires a SimulatedClock that runs ~720x real
// time so the four mood states cycle through in about a minute of wall clock.

#include <Arduino.h>
#include <array>

#if !defined(SIMULATOR)
#  include <WiFi.h>
#  include <time.h>
#  if __has_include("wifi_credentials.h")
#    include "wifi_credentials.h"
#  endif
#  ifndef WIFI_SSID
#    define WIFI_SSID ""
#  endif
#  ifndef WIFI_PASS
#    define WIFI_PASS ""
#  endif
#endif

#include "adapters/ArduinoClock.h"
#include "adapters/LvglDisplay.h"
#include "adapters/NoopNetwork.h"
#include "adapters/SimulatedClock.h"
#include "application/DisplayCoordinator.h"
#include "application/FeedingService.h"
#include "domain/Mood.h"
#include "domain/MoodCalculator.h"

#if defined(SIMULATOR)
#  include "adapters/NoopPreferences.h"
#  include "adapters/NoopStorage.h"
#else
#  include "adapters/LittleFsStorage.h"
#  include "adapters/NvsPreferences.h"
#endif

#if defined(SIMULATOR)
#  include "adapters/StubTapSensor.h"
#else
#  include "adapters/Cst816TapSensor.h"
#  include "adapters/EncoderButtonSensor.h"
#  include "adapters/LedRing.h"
#endif

namespace {

// 5 hours per the design.
constexpr int64_t HUNGRY_THRESHOLD_SEC = 5 * 3600;

#if defined(SIMULATOR)
// Pretend "now" started at a fixed epoch on Apr 26 2026 12:00 UTC.
// Fast-forward factor of 720 = 12 minutes per real second; full Happy→Hungry
// arc takes ~25s of wall clock.
feedme::adapters::SimulatedClock simClock(1745668800LL, 720);
feedme::ports::IClock& appClock = simClock;
#else
feedme::adapters::ArduinoClock realClock;
feedme::ports::IClock& appClock = realClock;
#endif

feedme::adapters::LvglDisplay display;
feedme::adapters::NoopNetwork network;
#if defined(SIMULATOR)
feedme::adapters::NoopStorage     storage;
feedme::adapters::NoopPreferences prefs;
#else
feedme::adapters::LittleFsStorage storage;
feedme::adapters::NvsPreferences  prefs;
#endif
#if defined(SIMULATOR)
feedme::adapters::StubTapSensor       taps;
feedme::adapters::StubTapSensor       button;
#else
feedme::adapters::Cst816TapSensor     taps;     // capacitive screen
feedme::adapters::EncoderButtonSensor button;   // physical knob press
feedme::adapters::LedRing             leds;     // 5-LED WS2812 ring
#endif

constexpr uint32_t LED_FEED_COLOR    = 0x00FF40;  // green
constexpr uint32_t LED_SNOOZE_COLOR  = 0x6644FF;  // purple
constexpr uint32_t LED_HISTORY_COLOR = 0x00AAFF;  // cyan

#if !defined(SIMULATOR)
// Bring up Wi-Fi (using build-flag credentials from wifi_credentials.h)
// and request an SNTP sync. Blocks up to ~15 s for association +
// up to ~5 s for the first NTP packet. Best-effort — failures are
// logged and the firmware continues; ArduinoClock falls back to
// millis()/1000 if time(nullptr) never crosses 2020.
void connectWifiAndSyncTime() {
    if (WIFI_SSID[0] == '\0') {
        Serial.println("[wifi] no credentials provided — skipping "
                       "(see firmware/include/wifi_credentials.h.example)");
        return;
    }
    Serial.printf("[wifi] connecting to '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    const uint32_t assocDeadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < assocDeadline) {
        delay(250);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[wifi] connect timeout — continuing offline");
        return;
    }
    Serial.printf("[wifi] connected, ip=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());

    // SNTP. UTC; the cat doesn't care about TZ.
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    const uint32_t ntpDeadline = millis() + 5000;
    while (time(nullptr) < 1577836800 && millis() < ntpDeadline) {
        delay(100);
    }
    const time_t now = time(nullptr);
    if (now >= 1577836800) {
        Serial.printf("[ntp] synced, epoch=%lld\n",
                      static_cast<long long>(now));
    } else {
        Serial.println("[ntp] sync timeout — continuing without real time");
    }
}
#endif

// Ambient breathing colour per mood. 0 = LEDs off.
uint32_t ambientColorFor(feedme::domain::Mood m) {
    using M = feedme::domain::Mood;
    switch (m) {
        case M::Happy:   return 0x22FF44;  // green
        case M::Neutral: return 0xFFEE00;  // yellow
        case M::Warning: return 0xFF8800;  // orange
        case M::Hungry:  return 0xFF1A1A;  // red
        case M::Fed:     return 0x22FF44;  // green
        case M::Sleepy:  return 0x6644FF;  // purple
    }
    return 0;
}

constexpr int SNOOZE_DURATION_SEC      = 30 * 60;  // long-press = 30 minutes
constexpr int THRESHOLD_STEP_SEC       = 30 * 60;  // one detent = ±30 min

feedme::application::FeedingService feeding(appClock, network, storage);
feedme::application::DisplayCoordinator displayCoord(
    display, feeding, appClock, prefs, HUNGRY_THRESHOLD_SEC);

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

#if !defined(SIMULATOR)
    connectWifiAndSyncTime();
#endif

    network.begin();
    storage.begin();
    prefs.begin();
    displayCoord.loadPreferences();
    feeding.loadHistoryFromStorage();
    taps.begin();
    button.begin();
#if !defined(SIMULATOR)
    leds.begin();
#endif

    // All eight input events route through one handler. Quick gestures
    // log a feed, deliberate ones snooze, the rest are placeholders
    // ready for menu / history / threshold tuning when those land.
    //
    //   Tap          (capacitive)  -> log feed
    //   Press        (physical)    -> log feed (alt tactile path)
    //   DoubleTap    (capacitive)  -> history view  (TODO)
    //   DoublePress  (physical)    -> history view  (TODO, alt path)
    //   LongTouch    (capacitive)  -> menu / settings (TODO)
    //   LongPress    (physical)    -> snooze 30 min
    //   RotateCW     (knob)        -> menu next / +1   (TODO)
    //   RotateCCW    (knob)        -> menu prev / -1   (TODO)
    auto handleInput = [](feedme::ports::TapEvent ev) {
        using E = feedme::ports::TapEvent;

        // Auto-dismiss the history overlay on any non-history gesture.
        if (display.historyVisible() &&
            ev != E::DoubleTap && ev != E::DoublePress) {
            display.setHistoryVisible(false);
        }

        switch (ev) {
            case E::Tap:
            case E::Press:
                Serial.println("[input] -> log feed");
                feeding.logFeeding("user");
#if !defined(SIMULATOR)
                leds.pulse(LED_FEED_COLOR);
#endif
                break;
            case E::DoubleTap:
            case E::DoublePress: {
                // Toggle: a second double-gesture dismisses the overlay.
                if (display.historyVisible()) {
                    display.setHistoryVisible(false);
                    Serial.println("[input] -> history (dismiss)");
                    break;
                }
                Serial.println("[input] -> history");
#if !defined(SIMULATOR)
                leds.pulse(LED_HISTORY_COLOR, 300);
#endif
                std::array<feedme::application::FeedingService::HistoryEntry,
                           feedme::application::FeedingService::HISTORY_CAPACITY> recent;
                const size_t n = feeding.copyRecentEvents(recent);
                feedme::adapters::HistoryItem items[
                    feedme::adapters::LvglDisplay::HISTORY_MAX]{};
                const int64_t now = appClock.nowSec();
                for (size_t i = 0; i < n && i < (size_t)feedme::adapters::LvglDisplay::HISTORY_MAX; ++i) {
                    const auto& e = recent[i];
                    items[i].ts = e.ts;
                    int64_t agoSec = (e.ts > 0 && now > e.ts) ? (now - e.ts) : 0;
                    int agoMin = static_cast<int>(agoSec / 60);
                    if (agoMin < 60) {
                        snprintf(items[i].line, sizeof(items[i].line),
                                 "%dm  %s", agoMin, e.type.c_str());
                    } else {
                        snprintf(items[i].line, sizeof(items[i].line),
                                 "%dh%02dm  %s",
                                 agoMin / 60, agoMin % 60, e.type.c_str());
                    }
                    Serial.printf("  [%zu] ts=%lld type=%s by=%s\n",
                                  i, static_cast<long long>(e.ts),
                                  e.type.c_str(), e.by.c_str());
                }
                display.setHistory(items, static_cast<int>(n));
                display.setHistoryVisible(true);
                break;
            }
            case E::LongTouch:
                Serial.println("[input] -> menu (TODO)");
                break;
            case E::LongPress:
                Serial.println("[input] -> snooze 30m");
                feeding.snooze("user", SNOOZE_DURATION_SEC);
#if !defined(SIMULATOR)
                leds.pulse(LED_SNOOZE_COLOR);
#endif
                break;
            case E::RotateCW: {
                const int64_t v = displayCoord.adjustHungryThreshold(+THRESHOLD_STEP_SEC);
                Serial.printf("[input] -> threshold +30m (now %lldh %02lldm)\n",
                              v / 3600, (v % 3600) / 60);
                break;
            }
            case E::RotateCCW: {
                const int64_t v = displayCoord.adjustHungryThreshold(-THRESHOLD_STEP_SEC);
                Serial.printf("[input] -> threshold -30m (now %lldh %02lldm)\n",
                              v / 3600, (v % 3600) / 60);
                break;
            }
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
#if !defined(SIMULATOR)
    leds.tick();
#endif

    if (now - lastServiceTickMs >= 1000) {
        lastServiceTickMs = now;
        feeding.tick();

#if !defined(SIMULATOR)
        // Update ambient LED-ring colour when the mood changes.
        static feedme::domain::Mood lastMood = feedme::domain::Mood::Happy;
        static bool lastMoodValid = false;
        const feedme::domain::Mood mood = feedme::domain::calculateMood(
            feeding.state(), appClock.nowSec(), displayCoord.hungryThresholdSec());
        if (!lastMoodValid || mood != lastMood) {
            lastMoodValid = true;
            lastMood = mood;
            leds.setAmbient(ambientColorFor(mood));
        }
#endif
    }

    displayCoord.tick();
    delay(5);
}
