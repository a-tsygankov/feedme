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
#include "application/SyncService.h"
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

// ── Display sleep / wake (PowerManager) ───────────────────────────
//
// The LCD backlight (TFT_BL = GPIO 46, active HIGH on this board)
// is the dominant power draw at idle. After SleepTimeout::minutes()
// minutes of inactivity we drive it LOW; any subsequent input wakes
// it back to HIGH and is then CONSUMED — does not propagate to the
// active view — so the user doesn't accidentally trigger menu
// navigation just by waking. Wake also forces a transition to
// "idle" so we always wake on the main page, regardless of which
// screen was visible when sleep happened.
//
// timeoutMin == 0 disables sleep entirely. Inputs always reset the
// idle timer (whether sleeping or not) via powerNotifyInput().
//
// Phase E: sleep-entry + wake-entry SYNC GATES. Per docs/sync-implementation-handoff.md
// §2 Q3, when the device is about to drop the LCD (sleep-entry) or just
// brought it back up (wake-entry), it checks `now - lastSyncAt vs
// syncIntervalSec`. If exceeded, it transitions to the SyncingView
// (which renders "Syncing..." and runs syncFull synchronously) BEFORE
// completing the power-state change. The `pendingSleepAfterSync` flag
// holds the sleep transition until the SyncingView returns to idle —
// the loop sees the active view become "idle" again, and only then
// turns off the backlight. Without that hold, the user would see the
// screen go dark mid-sync.
struct PowerState {
    uint32_t lastInputMs           = 0;
    bool     sleeping              = false;
    // Phase E — set by powerTickAndMaybeSleep when the gate triggered
    // a sync: we deferred the actual backlight-off until the SyncingView
    // exits. The loop polls `display.currentView()` and, when it's no
    // longer "syncing", drops the backlight here.
    bool     sleepAfterSync        = false;
    // Wake-entry gate fires once per wake. Set in powerWakeIfSleeping;
    // the loop runs maybeSync() once on the next tick (so the user
    // sees the existing wake view first), then clears.
    bool     wakeSyncPending       = false;
};
PowerState powerState;

void powerNotifyInput() {
    powerState.lastInputMs = millis();
}
// powerWakeIfSleeping / powerTickAndMaybeSleep are defined at global
// scope below; their callers (the input lambda + loop()) are also at
// global scope inside setup() / loop(), which lookup them fine.

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

// Phase C — sync service. Owns the pair lifecycle + /api/sync round-
// trip. WifiNetwork is the only adapter wired in; the cat + user
// rosters are read-and-replaced in place on each successful sync.
// On the simulator path WifiNetwork is replaced by NoopNetwork in
// the conditional above; SyncService still constructs but its HTTP
// calls return network errors (sim is offline by design).
#if !defined(SIMULATOR)
feedme::application::SyncService syncService(
    wifiNetwork, display.roster(), display.userRoster());
#endif

// "Are we paired?" — flipped to true when the device picks up a
// DeviceToken from /api/pair/check. HomeView watches a pointer to
// this so the Sync row can grey out until pairing completes.
bool gIsPaired = false;

uint32_t lastServiceTickMs = 0;

}  // namespace

bool powerWakeIfSleeping() {
    if (!powerState.sleeping) return false;
#if !defined(SIMULATOR)
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
#endif
    powerState.sleeping = false;
    powerState.lastInputMs = millis();
    Serial.println("[power] wake -> idle");
    // Always wake on the main page. If the device fell asleep on
    // FeedConfirm / Settings / etc., the user wakes back on Idle so
    // they don't have to navigate out of a forgotten context.
    display.transitionTo("idle");
    // Phase E — flag the wake-entry sync gate. We don't kick the sync
    // here because the user just touched the knob and we want them to
    // SEE the idle view first (responsiveness > stale-data correctness).
    // The next loop tick checks the flag and runs maybeSync(), at which
    // point the SyncingView takes over the screen if the gate triggers.
    powerState.wakeSyncPending = true;
    return true;
}

// Phase E — Helper. Returns true if the sync gate just triggered AND
// we transitioned to the syncing view to handle it. Caller can use
// the return value to decide whether to defer follow-up state changes.
static bool maybeSyncViaSyncingView() {
#if !defined(SIMULATOR)
    if (!syncService.isPaired()) return false;
    const int64_t nowSec = appClock.nowSec();
    if (nowSec <= 0) return false;  // clock not yet sync'd; gate is meaningless
    const int interval = syncService.syncIntervalSec();
    const int64_t elapsed = nowSec - syncService.lastSyncAt();
    if (interval <= 0) return false;
    if (syncService.lastSyncAt() > 0 && elapsed < interval) return false;
    // Gate would trigger — show the splash. SyncingView's first
    // render() calls syncService.syncFull() and on completion its
    // nextView() returns "idle", which puts us back on the dashboard.
    Serial.printf("[power] sync gate -> SyncingView (elapsed=%lld interval=%d)\n",
                  static_cast<long long>(elapsed), interval);
    display.transitionTo("syncing");
    return true;
#else
    (void)0;
    return false;
#endif
}

void powerTickAndMaybeSleep() {
    if (powerState.sleeping) return;
    const int timeoutMin = display.sleepTimeout().minutes();
    if (timeoutMin <= 0) return;  // 0 = never sleep
    // Skip sleep while the in-place captive portal is active — the
    // user is mid-flow on their phone and the device-side splash
    // showing AP info needs to stay visible.
#if !defined(SIMULATOR)
    if (captivePortal.state() != feedme::adapters::WifiCaptivePortal::State::Idle) {
        return;
    }
#endif

    // Phase E — if a previous sleep tick deferred the backlight-off
    // because the gate kicked a sync, finish the deferred sleep once
    // the SyncingView has returned to idle.
    if (powerState.sleepAfterSync) {
        const char* cur = display.currentView();
        if (cur && strcmp(cur, "syncing") == 0) return;  // still syncing — keep waiting
#if !defined(SIMULATOR)
        digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
#endif
        powerState.sleeping       = true;
        powerState.sleepAfterSync = false;
        Serial.println("[power] deferred sleep -> backlight off");
        return;
    }

    const uint32_t timeoutMs = static_cast<uint32_t>(timeoutMin) * 60u * 1000u;
    if (millis() - powerState.lastInputMs >= timeoutMs) {
        // Phase E — sleep-entry sync gate. Try to transition to the
        // SyncingView FIRST; if it triggers, defer turning off the
        // backlight until that view returns. Otherwise sleep
        // immediately.
        if (maybeSyncViaSyncingView()) {
            powerState.sleepAfterSync = true;
            Serial.printf("[power] sleep deferred for sync after %d min idle\n", timeoutMin);
            return;
        }
#if !defined(SIMULATOR)
        digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON);
#endif
        powerState.sleeping = true;
        Serial.printf("[power] sleep after %d min idle\n", timeoutMin);
    }
}

// Phase E — wake-entry sync gate. Called once per loop tick after
// powerWakeIfSleeping has already brought the backlight back up.
// Defers a frame so the user sees the idle dashboard refresh before
// the SyncingView covers it; the user's wake gesture lands on a
// responsive screen.
void powerHandleWakeSync() {
    if (!powerState.wakeSyncPending) return;
    powerState.wakeSyncPending = false;
    (void)maybeSyncViaSyncingView();
}

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
    // Auto-generate hid on first boot (and after a pairing reset). The
    // device picks its own identifier — `feedme-{mac6}` for the first
    // generation, `feedme-{mac6}-{n}` after `n` pairing resets — so
    // each reset cycle produces a NEW backend household and the
    // orphaned old one stays harmless. Stable thereafter: stored in
    // NVS, used for every /api/* call until the next reset.
    {
        char nvsHid[40]{};
        const bool haveHid = prefs.getHid(nvsHid, sizeof(nvsHid))
                             && nvsHid[0] != '\0';
        if (!haveHid) {
            uint8_t mac[6];
            WiFi.macAddress(mac);
            const int resetCount = prefs.getHidResetCount(0);
            char generated[40]{};
            if (resetCount <= 0) {
                snprintf(generated, sizeof(generated),
                         "feedme-%02x%02x%02x%02x%02x%02x",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            } else {
                snprintf(generated, sizeof(generated),
                         "feedme-%02x%02x%02x%02x%02x%02x-%d",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                         resetCount);
            }
            prefs.setHid(generated);
            Serial.printf("[setup] generated hid='%s' (reset=%d)\n",
                          generated, resetCount);
            // Re-read into nvsHid so the rest of setup uses it.
            std::strncpy(nvsHid, generated, sizeof(nvsHid) - 1);
        }
        wifiNetwork.setHid(nvsHid);
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
    display.sleepTimeout().loadFromStorage(
        prefs.getSleepTimeoutMin(feedme::domain::SleepTimeout::DEFAULT_MIN));

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
            // Phase C: pull the sync timestamps too. Default 0 is the
            // "loaded from a pre-Phase-C snapshot" sentinel — the
            // first sync overwrites with server-canonical timestamps.
            const int64_t catCreatedAt = prefs.getCatCreatedAt(i, 0);
            const int64_t catUpdatedAt = prefs.getCatUpdatedAt(i, 0);
            // Phase D: stable per-cat uuid (32-hex). Empty string =
            // "no uuid yet" — server backfills on first sync.
            char catUuidBuf[feedme::domain::Cat::UUID_CAP] = {0};
            const bool haveCatUuid = prefs.getCatUuid(i, catUuidBuf, sizeof(catUuidBuf));
            roster.appendLoaded(static_cast<uint8_t>(id),
                                haveName ? nameBuf : nullptr,
                                haveSlug ? slugBuf : nullptr,
                                portion,
                                threshold,
                                color,
                                catCreatedAt,
                                catUpdatedAt,
                                haveCatUuid ? catUuidBuf : nullptr);
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
            const int64_t userCreatedAt = prefs.getUserCreatedAt(i, 0);
            const int64_t userUpdatedAt = prefs.getUserUpdatedAt(i, 0);
            char userUuidBuf[feedme::domain::User::UUID_CAP] = {0};
            const bool haveUserUuid = prefs.getUserUuid(i, userUuidBuf, sizeof(userUuidBuf));
            roster.appendLoaded(static_cast<uint8_t>(id),
                                haveName ? nameBuf : nullptr,
                                color,
                                userCreatedAt,
                                userUpdatedAt,
                                haveUserUuid ? userUuidBuf : nullptr);
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
        // Restore the persisted "last user who fed" — picks the
        // FeederPicker's default focus so multi-user homes don't
        // re-pick from scratch on every feed.
        roster.loadLastFeederIdx(prefs.getLastFeederIdx(0));
    }
    display.pouringView().setFeedingService(&feeding);
    display.pouringView().setUserRoster(&display.userRoster());
    display.settingsView().setNetwork(&network);
    display.settingsView().setCoordinator(&displayCoord);
    display.thresholdEditView().setCoordinator(&displayCoord);
    display.lockConfirmView().setSensors(&taps, &button);
    display.menuView().setTouchSensor(&taps);
#if !defined(SIMULATOR)
    display.wifiSwitchView().setPortal(&captivePortal);
#endif
    display.wifiResetView().setNetwork(&network);
    display.wifiResetView().setOnConfirm(+[]() {
        // In-place AP+STA: bring up the SoftAP alongside the existing
        // STA connection. Existing feed/snooze/mood state is preserved
        // — no reboot. WifiSwitchView watches the portal state and
        // bounces back to settings on success/failure. The AP is torn
        // down by WifiSwitchView::onLeave.
#if !defined(SIMULATOR)
        Serial.println("[wifi] starting in-place portal (AP+STA)");
        captivePortal.beginInPlace(prefs);
#else
        Serial.println("[wifi] switch (sim) — no portal");
#endif
    });

    // ── Pairing screen wiring ─────────────────────────────────────────
    // PairingView shows the QR code that deep-links into the webapp's
    // /setup flow with the device's hid pre-filled. Shown on first boot
    // (and after any "Reset pairing" cycle) until the user taps to
    // dismiss it; that tap fires the onPaired callback below which
    // flips the NVS paired flag so the screen doesn't re-appear.
    //
    // The hid + URL strings are kept in static buffers so the view can
    // hold pointers for its lifetime without copying.
#if !defined(SIMULATOR) && defined(FEEDME_BACKEND_URL)
    {
        static char gPairHid[40] = {0};
        static char gPairUrl[160] = {0};
        char nvsHid[40]{};
        const bool haveHid = prefs.getHid(nvsHid, sizeof(nvsHid))
                             && nvsHid[0] != '\0';
        if (haveHid) {
            std::strncpy(gPairHid, nvsHid, sizeof(gPairHid) - 1);
            // Web app origin — distinct from the Worker origin because
            // the static SPA + the API live on different Cloudflare
            // surfaces. Hard-coded for now; could be a build flag later.
            snprintf(gPairUrl, sizeof(gPairUrl),
                     "https://feedme-webapp.pages.dev/setup?hid=%s",
                     gPairHid);
        }
        display.pairingView().setHid(gPairHid);
        display.pairingView().setUrl(gPairUrl);
        // Post PR #34: PairingView OWNS the pair-handshake polling
        // loop (calls /pair/start on entry, polls /pair/check, transitions
        // to "syncing" on confirmed). The tap-to-begin step is gone —
        // the user's only job is to scan + sign in on the webapp.
        // onPaired fires when /pair/check returns confirmed; main.cpp
        // uses it to flip the runtime paired flag.
        display.pairingView().setOnPaired(+[]() {
            Serial.println("[pairing] auto-paired via webapp sign-in");
            gIsPaired = true;
        });

        // ── Phase C: SyncService wiring ──────────────────────────
        // Load any cached deviceId / token / homeName from NVS. For
        // legacy devices that already have an `hid` (the MAC-derived
        // value) but no `deviceId` (the new key), migrate by setting
        // deviceId = hid. This preserves their server-side `devices`
        // table mapping created by PR #22's migration 0004.
        {
            char devBuf[40] = {0};
            if (!prefs.getDeviceId(devBuf, sizeof(devBuf)) || devBuf[0] == '\0') {
                // Empty deviceId → migrate from legacy hid (if any)
                // or generate a fresh 16-hex random for first boot.
                if (nvsHid[0] != '\0') {
                    std::strncpy(devBuf, nvsHid, sizeof(devBuf) - 1);
                    Serial.printf("[sync] migrating legacy hid '%s' → deviceId\n", devBuf);
                } else {
                    // Generate `feedme-` + 16 random hex chars.
                    snprintf(devBuf, sizeof(devBuf), "feedme-%04x%04x%04x%04x",
                             static_cast<unsigned>(esp_random() & 0xffff),
                             static_cast<unsigned>(esp_random() & 0xffff),
                             static_cast<unsigned>(esp_random() & 0xffff),
                             static_cast<unsigned>(esp_random() & 0xffff));
                    Serial.printf("[sync] generated new deviceId '%s'\n", devBuf);
                }
                prefs.setDeviceId(devBuf);
            }
            syncService.setDeviceId(devBuf);

            char tokBuf[256] = {0};
            if (prefs.getDeviceToken(tokBuf, sizeof(tokBuf)) && tokBuf[0] != '\0') {
                syncService.setDeviceToken(tokBuf);
                gIsPaired = true;
                Serial.println("[sync] loaded device token from NVS — paired");
            }

            char homeBuf[64] = {0};
            if (prefs.getHomeName(homeBuf, sizeof(homeBuf)) && homeBuf[0] != '\0') {
                syncService.setHomeName(homeBuf);
            }
            syncService.setLastSyncAt(prefs.getLastSyncAt(0));
            // Phase E — restore last-known sync interval (in seconds)
            // from NVS so the FIRST sleep-entry gate after boot has a
            // sensible value before the first /api/sync overwrites it.
            // 14400 (4 h) matches backend DEFAULT_SYNC_INTERVAL_SEC.
            syncService.setSyncIntervalSec(prefs.getSyncIntervalSec(14400));
        }

        // Wire the new pairing/sync views to the service + prefs.
        // PairingView itself now owns the pair-handshake poll loop
        // (post PR #34); PairingProgressView is kept registered for
        // back-compat but is no longer reachable from the QR screen.
        display.pairingView().setSyncService(&syncService);
        display.pairingView().setPreferences(&prefs);
        display.pairingProgressView().setSyncService(&syncService);
        display.pairingProgressView().setPreferences(&prefs);
        display.syncingView().setSyncService(&syncService);
        // Phase F — LoginQrView posts to /api/auth/login-token-create
        // via SyncService and renders the resulting one-shot URL as
        // a QR. Same DeviceToken flow as syncing/unpair.
        display.loginQrView().setSyncService(&syncService);
        display.homeView().setIsPairedSource(&gIsPaired);

        // Reset-pairing confirmation (Phase C rewrite). On confirm:
        //   1. DELETE /api/pair/<deviceId> with the current device
        //      token (best-effort; ignore network errors so a Reset
        //      while offline still proceeds locally).
        //   2. Wipe NVS: device token, paired flag, home name, hid,
        //      cat/user counts, last-sync timestamp.
        //   3. Generate a fresh 16-hex random deviceId per Q5 in the
        //      sync handoff doc.
        //   4. Reboot — the new boot regenerates rosters from scratch
        //      (one default cat + user) and PairingView shows the new QR.
        display.resetPairConfirmView().setOnConfirm(+[]() {
            Serial.println("[pairing] reset confirmed — unpair + wipe + reboot");
            // Step 1: server-side unpair (no-op if already unpaired).
            (void)syncService.unpair();
            // Step 2: wipe local pairing + roster state.
            prefs.setDeviceToken("");
            prefs.setPaired(false);
            prefs.setHomeName("");
            prefs.setHid("");
            prefs.setLastSyncAt(0);
            prefs.setCatCount(0);
            prefs.setUserCount(0);
            // Step 3: rotate to a fresh deviceId so the orphaned
            // server-side devices row can't be replayed against.
            char devBuf[40];
            snprintf(devBuf, sizeof(devBuf), "feedme-%04x%04x%04x%04x",
                     static_cast<unsigned>(esp_random() & 0xffff),
                     static_cast<unsigned>(esp_random() & 0xffff),
                     static_cast<unsigned>(esp_random() & 0xffff),
                     static_cast<unsigned>(esp_random() & 0xffff));
            prefs.setDeviceId(devBuf);
            Serial.printf("[pairing] new deviceId='%s' — rebooting\n", devBuf);
            delay(300);
            ESP.restart();
        });

        // First boot? Have BootView land on the pairing screen instead
        // of idle so the QR is the first interactive thing the user
        // sees. Once paired (after the first tap), BootView reverts to
        // its default "idle" landing on every subsequent boot.
        if (haveHid && !prefs.getPaired(false)) {
            Serial.println("[pairing] device unpaired — showing QR after boot");
            display.bootView().setNext("pairing");
        }
    }
#endif
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

        // PowerManager: any input wakes the screen and is then
        // consumed — does not propagate to the active view. This
        // satisfies "wake doesn't accidentally trigger Feed". The
        // wake also forces a transition to Idle (main page).
        if (powerWakeIfSleeping()) {
            return;
        }
        powerNotifyInput();

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

    powerState.lastInputMs = millis();   // start the idle timer fresh

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
    // Pump the captive portal whenever it's running an in-place switch.
    // (Boot-mode portal runs in its own blocking loop in setup() and
    // never reaches here.) The state machine inside handle() advances
    // the deferred disconnect/reconnect and watches WiFi.status().
    if (captivePortal.state() != feedme::adapters::WifiCaptivePortal::State::Idle) {
        captivePortal.handle();
    }
#endif

    if (now - lastServiceTickMs >= 1000) {
        lastServiceTickMs = now;
        feeding.tick();
        // Phase E — wake-entry gate runs BEFORE the sleep tick so a
        // user who taps to wake doesn't immediately get sleep-entry'd
        // again on the same second.
        powerHandleWakeSync();
        powerTickAndMaybeSleep();   // backlight off after N min idle
        // Phase C — push the wall clock into the rosters so any
        // setter that mutates a cat/user this tick can stamp
        // updatedAt without reaching for a global clock. 1 Hz cadence
        // is fine for LWW sync resolution (which only cares about
        // ordering, not microsecond precision).
        display.roster().setNow(appClock.nowSec());
        display.userRoster().setNow(appClock.nowSec());
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
        if (display.sleepTimeout().consumeDirty()) {
            prefs.setSleepTimeoutMin(display.sleepTimeout().minutes());
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
                // Phase C — sync timestamps. The cat's updatedAt was
                // bumped by whichever setter triggered the dirty flag.
                prefs.setCatCreatedAt(i, roster.at(i).createdAt);
                prefs.setCatUpdatedAt(i, roster.at(i).updatedAt);
                // Phase D — uuid. Always write (only changes after a
                // sync response; no-op-on-disk handled inside NVS).
                prefs.setCatUuid(i, roster.at(i).uuid);
            }
        }
        if (display.userRoster().consumeDirty()) {
            const auto& users = display.userRoster();
            prefs.setUserCount(users.count());
            for (int i = 0; i < users.count(); ++i) {
                prefs.setUserId   (i, users.at(i).id);
                prefs.setUserName (i, users.at(i).name);
                prefs.setUserColor(i, users.at(i).avatarColor);
                prefs.setUserCreatedAt(i, users.at(i).createdAt);
                prefs.setUserUpdatedAt(i, users.at(i).updatedAt);
                prefs.setUserUuid (i, users.at(i).uuid);
            }
        }
        if (display.userRoster().consumeLastFeederDirty()) {
            prefs.setLastFeederIdx(display.userRoster().lastFeederIdx());
        }
#if !defined(SIMULATOR)
        // Phase E — persist lastSyncAt + syncIntervalSec whenever the
        // SyncService observes a new value. We diff against the
        // previously-saved value so the every-tick prefs.getInt is
        // skipped on no-op writes (NVS putInt is itself a no-op when
        // the stored value matches, but the call still costs a flash
        // read; better to short-circuit here).
        {
            static int64_t lastSavedSyncAt    = -1;
            static int     lastSavedInterval  = -1;
            const int64_t curSyncAt   = syncService.lastSyncAt();
            const int     curInterval = syncService.syncIntervalSec();
            if (curSyncAt > 0 && curSyncAt != lastSavedSyncAt) {
                prefs.setLastSyncAt(curSyncAt);
                lastSavedSyncAt = curSyncAt;
            }
            if (curInterval > 0 && curInterval != lastSavedInterval) {
                prefs.setSyncIntervalSec(curInterval);
                lastSavedInterval = curInterval;
            }
        }
#endif
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
