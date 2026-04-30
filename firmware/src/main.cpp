// Composition root. The only file that knows about concrete adapters.
//
// On the simulator (-DSIMULATOR), wires a SimulatedClock that runs ~720x real
// time so the four mood states cycle through in about a minute of wall clock.

#include <Arduino.h>
#include <array>
#include <cstring>

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
#if !defined(SIMULATOR)
#  include "adapters/WifiCaptivePortal.h"
#  include "adapters/WifiNetwork.h"
#endif
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

// (HUNGRY_THRESHOLD_SEC global removed — threshold is per-cat now;
// the default lives in Cat::DEFAULT_THRESHOLD_S.)

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

// Network adapter: WifiNetwork against the Cloudflare Worker when
// FEEDME_BACKEND_URL is set; otherwise NoopNetwork (link-state only,
// no fetch/post). The hid is set at boot from NVS (captured by the
// captive portal) or — if NVS is empty — from the FEEDME_HID build
// flag fallback. WifiNetwork's hid_ starts empty if neither source
// has it, in which case fetchState/postFeed early-return.
#if !defined(SIMULATOR) && defined(FEEDME_BACKEND_URL)
#  if !defined(FEEDME_HID)
#    define FEEDME_HID ""
#  endif
feedme::adapters::WifiNetwork wifiNetwork(FEEDME_BACKEND_URL, FEEDME_HID);
feedme::ports::INetwork&      network = wifiNetwork;
#else
feedme::adapters::NoopNetwork noopNetwork;
feedme::ports::INetwork&      network = noopNetwork;
#endif
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
feedme::adapters::WifiCaptivePortal captivePortal;

// Bring up Wi-Fi using the supplied creds and request an SNTP sync.
// Blocks up to ~15 s for association + ~5 s for the first NTP packet.
// Best-effort — failures are logged and the firmware continues;
// ArduinoClock falls back to millis()/1000 if time never crosses 2020.
void connectWifiAndSyncTime(const char* ssid, const char* pass) {
    if (!ssid || ssid[0] == '\0') {
        Serial.println("[wifi] no credentials — skipping connect");
        return;
    }
    Serial.printf("[wifi] connecting to '%s'...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass ? pass : "");

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

    // SNTP. UTC; TimeZone offset shifts to local for display only.
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

// Runs the captive-portal setup loop until the user submits the form.
// Then writes captured creds to NVS (already done inside the portal)
// and reboots so the next boot picks up the saved creds in STA mode.
// This function never returns.
[[noreturn]] void runCaptivePortalAndReboot() {
    captivePortal.begin(prefs);
    display.setupView().setApName(captivePortal.apName());
    display.setupView().setUrl   (captivePortal.apIp());
    display.transitionTo("setup");

    Serial.printf("[setup] portal active — connect to '%s' then open http://%s\n",
                  captivePortal.apName(), captivePortal.apIp());

    while (!captivePortal.isComplete()) {
        captivePortal.handle();
        display.tick();   // pump LVGL so SetupView stays responsive
        delay(2);
    }

    Serial.println("[setup] saved — rebooting");
    delay(500);  // give the HTTP "saved" response a chance to flush
    ESP.restart();
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

// FeedingService takes the cat roster so events stamp Cat::id and
// each cat keeps its own state. DisplayCoordinator reads the active
// cat's state via roster.activeCatIdx().
feedme::application::FeedingService feeding(
    appClock, network, storage, display.roster());
// Threshold is now per-cat (Phase E.x); DisplayCoordinator reads it
// from the active cat. The default lives in Cat::DEFAULT_THRESHOLD_S.
// Timezone offset shifts UTC epoch into local hour/minute for display.
feedme::application::DisplayCoordinator displayCoord(
    display, feeding, appClock, display.roster(), display.timezone());

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

    // Mount LittleFS BEFORE LVGL init — display.begin() registers an
    // lv_fs driver pointing at LittleFS and views eagerly open image
    // files in their build() to read PNG header dimensions.
    storage.begin();

    display.begin();
    Serial.println("[feedme] display ready");

    // Prefs must come up before the Wi-Fi decision so we can read
    // captured-from-portal credentials.
    prefs.begin();

#if !defined(SIMULATOR)
    {
        // Boot decision: NVS creds → STA. If NVS is empty AND a
        // build-flag fallback exists (dev convenience for keeping
        // wifi_credentials.h working) → STA with that. Otherwise
        // captive portal (never returns; reboots after submit).
        char nvsSsid[32]{};
        char nvsPass[64]{};
        const bool haveNvsSsid = prefs.getWifiSsid(nvsSsid, sizeof(nvsSsid))
                                 && nvsSsid[0] != '\0';
        if (haveNvsSsid) {
            prefs.getWifiPass(nvsPass, sizeof(nvsPass));
            connectWifiAndSyncTime(nvsSsid, nvsPass);
        } else if (WIFI_SSID[0] != '\0') {
            Serial.println("[setup] no NVS Wi-Fi creds — using build flag");
            connectWifiAndSyncTime(WIFI_SSID, WIFI_PASS);
        } else {
            Serial.println("[setup] no Wi-Fi creds anywhere — captive portal");
            runCaptivePortalAndReboot();   // never returns
        }
    }

#  if defined(FEEDME_BACKEND_URL)
    // Override WifiNetwork's hid with the NVS value if one was
    // captured at setup. Falls back to the build-flag default
    // already in the constructor.
    {
        char nvsHid[32]{};
        if (prefs.getHid(nvsHid, sizeof(nvsHid)) && nvsHid[0] != '\0') {
            wifiNetwork.setHid(nvsHid);
        }
    }
    // Give WifiNetwork a live reference to the TimeZone so each
    // /api/state poll appends the user's offset. Backend uses it
    // for the local-day boundary on todayCount.
    wifiNetwork.setTimeZone(&display.timezone());
#  endif
#endif

    network.begin();
    // (DisplayCoordinator::loadPreferences removed — threshold is now
    // per-cat and seeded inside the roster load block below.)
    // Wire FeedingService into PouringView so a completed pour logs
    // through the application layer. PouringView owns the only call
    // site for logFeeding now that the dispatcher's tap-to-feed path
    // has migrated. Per-cat portion is seeded below alongside the
    // rest of the cat roster fields.
    display.quiet().loadFromStorage(
        prefs.getQuietEnabled(false),
        prefs.getQuietStartHour  (feedme::domain::QuietWindow::DEFAULT_START_HOUR),
        prefs.getQuietStartMinute(feedme::domain::QuietWindow::DEFAULT_START_MINUTE),
        prefs.getQuietEndHour    (feedme::domain::QuietWindow::DEFAULT_END_HOUR),
        prefs.getQuietEndMinute  (feedme::domain::QuietWindow::DEFAULT_END_MINUTE));
    display.wake().loadFromStorage(
        prefs.getWakeHour(feedme::domain::WakeTime::DEFAULT_HOUR),
        prefs.getWakeMinute(feedme::domain::WakeTime::DEFAULT_MINUTE));
    display.timezone().loadFromStorage(
        prefs.getTimeZoneOffsetMin(feedme::domain::TimeZone::DEFAULT_MIN));

    // Cat roster — load count + per-slot fields, then ensure N≥1
    // (seedDefaultIfEmpty adds one default cat on first boot).
    //
    // Migration: the legacy global "portionG" NVS key (pre-per-cat)
    // seeds slot 0's portion if no per-slot value is yet stored, so
    // existing devices don't reset to default.
    {
        auto& roster = display.roster();
        roster.clear();
        const int n = prefs.getCatCount(0);
        const int legacyPortion = prefs.getPortionGrams(
            feedme::domain::PortionState::DEFAULT_G);
        const int64_t legacyThreshold = prefs.getHungryThresholdSec(
            feedme::domain::Cat::DEFAULT_THRESHOLD_S);
        for (int i = 0; i < n && i < feedme::domain::CatRoster::MAX_CATS; ++i) {
            const int  id = prefs.getCatId(i, i);
            char nameBuf[feedme::domain::Cat::NAME_CAP] = {0};
            char slugBuf[feedme::domain::Cat::SLUG_CAP] = {0};
            const bool haveName = prefs.getCatName(i, nameBuf, sizeof(nameBuf));
            const bool haveSlug = prefs.getCatSlug(i, slugBuf, sizeof(slugBuf));
            // Per-slot portion + threshold; migration: slot 0 falls back
            // to the legacy single keys so existing devices keep values.
            const int defaultPort = (i == 0) ? legacyPortion
                                              : feedme::domain::PortionState::DEFAULT_G;
            const int64_t defaultThr = (i == 0) ? legacyThreshold
                                                  : feedme::domain::Cat::DEFAULT_THRESHOLD_S;
            const int     portion   = prefs.getCatPortion(i, defaultPort);
            const int64_t threshold = prefs.getCatThresholdSec(i, defaultThr);
            // 0 sentinel → CatRoster falls back to autoCatColor(id).
            const uint32_t color    = prefs.getCatColor(i, 0);
            roster.appendLoaded(static_cast<uint8_t>(id),
                                haveName ? nameBuf : nullptr,
                                haveSlug ? slugBuf : nullptr,
                                portion,
                                threshold,
                                color);
            // Per-slot schedule hours layer on top of the defaults
            // already populated by appendLoaded → MealSchedule's ctor.
            // Reach into the cat we just appended (count_-1) and
            // overwrite each slot from NVS if a stored value exists.
            auto& cat = roster.at(roster.count() - 1);
            for (int s = 0; s < feedme::domain::MealSchedule::SLOT_COUNT; ++s) {
                const int defH = cat.schedule.slotHour(s);
                const int h    = prefs.getCatScheduleHour(i, s, defH);
                cat.schedule.setSlotHour(s, h);
            }
        }
        roster.seedDefaultIfEmpty();
        roster.markClean();
        // If we just seeded a default, persist immediately so the next
        // boot doesn't keep "first-run"-seeding (which would re-issue
        // ids and confuse any future event-attribution work).
        if (n == 0) {
            prefs.setCatCount(roster.count());
            for (int i = 0; i < roster.count(); ++i) {
                prefs.setCatId         (i, roster.at(i).id);
                prefs.setCatName       (i, roster.at(i).name);
                prefs.setCatSlug       (i, roster.at(i).slug);
                prefs.setCatPortion    (i, roster.at(i).portion.grams());
                prefs.setCatThresholdSec(i, roster.at(i).hungryThresholdSec);
                prefs.setCatColor      (i, roster.at(i).avatarColor);
                for (int s = 0; s < feedme::domain::MealSchedule::SLOT_COUNT; ++s) {
                    prefs.setCatScheduleHour(i, s, roster.at(i).schedule.slotHour(s));
                }
            }
        }
        // Restore last active cat. Clamps to a valid slot in case the
        // roster shrank (cat removed since the last save). Falls back
        // to slot 0 silently — the worst case is one extra rotate.
        const int storedActive = prefs.getActiveCatIdx(0);
        if (storedActive >= 0 && storedActive < roster.count()) {
            roster.setActiveCatIdx(storedActive);
        }
    }

    // User roster — same load + seed + persist-on-first-run pattern.
    {
        auto& roster = display.userRoster();
        roster.clear();
        const int n = prefs.getUserCount(0);
        for (int i = 0; i < n && i < feedme::domain::UserRoster::MAX_USERS; ++i) {
            const int  id = prefs.getUserId(i, i);
            char nameBuf[feedme::domain::User::NAME_CAP] = {0};
            const bool haveName = prefs.getUserName(i, nameBuf, sizeof(nameBuf));
            const uint32_t color = prefs.getUserColor(i, 0);
            roster.appendLoaded(static_cast<uint8_t>(id),
                                haveName ? nameBuf : nullptr,
                                color);
        }
        roster.seedDefaultIfEmpty();
        roster.markClean();
        if (n == 0) {
            prefs.setUserCount(roster.count());
            for (int i = 0; i < roster.count(); ++i) {
                prefs.setUserId   (i, roster.at(i).id);
                prefs.setUserName (i, roster.at(i).name);
                prefs.setUserColor(i, roster.at(i).avatarColor);
            }
        }
    }
    display.pouringView().setFeedingService(&feeding);
    display.pouringView().setUserRoster(&display.userRoster());
    display.settingsView().setNetwork(&network);
    display.settingsView().setCoordinator(&displayCoord);
    display.thresholdEditView().setCoordinator(&displayCoord);
    display.lockConfirmView().setSensors(&taps, &button);
    display.wifiResetView().setOnConfirm(+[]() {
        // Clear stored Wi-Fi creds + hid so next boot lands in the
        // captive-portal setup mode (the only escape hatch unless a
        // build-flag fallback is also set).
#if !defined(SIMULATOR)
        prefs.clearWifiCreds();
        Serial.println("[wifi] reset — NVS creds cleared, restarting");
        delay(100);          // let the print drain over USB-CDC
        ESP.restart();
#else
        Serial.println("[wifi] reset (sim) — no reboot");
#endif
    });
    feeding.loadHistoryFromStorage();
    taps.begin();
    button.begin();
#if !defined(SIMULATOR)
    leds.begin();
#endif

    // All eight input events route through one handler. Only the
    // history overlay (double-tap / double-press) is cross-cutting at
    // the dispatcher level — everything else, including the universal
    // "back up one level" gesture (long-press / long-touch), is
    // handled by ScreenManager via IView::parent(). The LockConfirm
    // view stays registered for any future explicit destructive flow
    // but is no longer auto-triggered by long-press.
    auto handleInput = [](feedme::ports::TapEvent ev) {
        using E = feedme::ports::TapEvent;

        // History overlay is a cross-cutting modal; it intercepts double-
        // gestures regardless of which view is active.
        if (ev == E::DoubleTap || ev == E::DoublePress) {
            if (display.historyVisible()) {
                display.setHistoryVisible(false);
                Serial.println("[input] -> history (dismiss)");
                return;
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
                // Multi-cat: prefix the cat name when the household has
                // 2+ cats (adaptive UI rule). With N=1 the cat is
                // implicit and the line stays the original "Xm feed".
                char catPrefix[20] = "";
                if (display.roster().count() >= 2) {
                    const int slot = display.roster().findSlotById(e.cat);
                    if (slot >= 0) {
                        snprintf(catPrefix, sizeof(catPrefix), "%s ",
                                 display.roster().at(slot).name);
                    }
                }
                if (agoMin < 60) {
                    snprintf(items[i].line, sizeof(items[i].line),
                             "%s%dm  %s", catPrefix, agoMin, e.type.c_str());
                } else {
                    snprintf(items[i].line, sizeof(items[i].line),
                             "%s%dh%02dm  %s",
                             catPrefix, agoMin / 60, agoMin % 60, e.type.c_str());
                }
            }
            display.setHistory(items, static_cast<int>(n));
            display.setHistoryVisible(true);
            return;
        }

        // History visible? Any other gesture dismisses it.
        if (display.historyVisible()) {
            display.setHistoryVisible(false);
            return;
        }

        // Otherwise: hand to the active view. It returns the next view
        // name (or null to stay) and ScreenManager has already transitioned
        // by the time handleInput returns.
        display.handleInput(ev);
    };
    taps.onEvent(handleInput);
    button.onEvent(handleInput);

    Serial.println("[feedme] setup complete");

#if defined(SIMULATOR)
    // Pretend the cat was fed an hour ago at boot, then never again, so we
    // start in Happy and walk the full arc as time accelerates.
    feeding.logFeeding("sim", 0);  // seed cat sits at slot 0
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
        if (display.quiet().consumeDirty()) {
            prefs.setQuietEnabled    (display.quiet().enabled());
            prefs.setQuietStartHour  (display.quiet().startHour());
            prefs.setQuietStartMinute(display.quiet().startMinute());
            prefs.setQuietEndHour    (display.quiet().endHour());
            prefs.setQuietEndMinute  (display.quiet().endMinute());
        }
        if (display.wake().consumeDirty()) {
            prefs.setWakeHour(display.wake().hour());
            prefs.setWakeMinute(display.wake().minute());
        }
        if (display.timezone().consumeDirty()) {
            prefs.setTimeZoneOffsetMin(display.timezone().offsetMin());
        }
        if (display.roster().consumeDirty()) {
            const auto& roster = display.roster();
            prefs.setCatCount(roster.count());
            for (int i = 0; i < roster.count(); ++i) {
                prefs.setCatId         (i, roster.at(i).id);
                prefs.setCatName       (i, roster.at(i).name);
                prefs.setCatSlug       (i, roster.at(i).slug);
                prefs.setCatPortion    (i, roster.at(i).portion.grams());
                prefs.setCatThresholdSec(i, roster.at(i).hungryThresholdSec);
                prefs.setCatColor      (i, roster.at(i).avatarColor);
                for (int s = 0; s < feedme::domain::MealSchedule::SLOT_COUNT; ++s) {
                    prefs.setCatScheduleHour(i, s, roster.at(i).schedule.slotHour(s));
                }
            }
        }
        if (display.userRoster().consumeDirty()) {
            const auto& users = display.userRoster();
            prefs.setUserCount(users.count());
            for (int i = 0; i < users.count(); ++i) {
                prefs.setUserId   (i, users.at(i).id);
                prefs.setUserName (i, users.at(i).name);
                prefs.setUserColor(i, users.at(i).avatarColor);
            }
        }
        // Active cat — persisted independently of the roster's dirty
        // flag so the IdleView selector spinning between cats doesn't
        // re-write the entire roster every tick. Tracks last-saved
        // value here; NVS putInt is itself a no-op when the stored
        // value matches, but skipping the call avoids the read+compare.
        {
            static int lastSavedActive = -1;
            const int cur = display.roster().activeCatIdx();
            if (cur >= 0 && cur != lastSavedActive) {
                prefs.setActiveCatIdx(cur);
                lastSavedActive = cur;
            }
        }

#if !defined(SIMULATOR)
        // Update ambient LED-ring colour when the mood changes. Mood
        // tracks the active cat — multi-cat households see the LEDs
        // re-tint on cat switch (rotate-on-Idle).
        static feedme::domain::Mood lastMood = feedme::domain::Mood::Happy;
        static bool lastMoodValid = false;
        const int activeSlot = display.roster().activeCatIdx();
        const feedme::domain::Mood mood = feedme::domain::calculateMood(
            feeding.state(activeSlot), appClock.nowSec(),
            displayCoord.hungryThresholdSec());
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
