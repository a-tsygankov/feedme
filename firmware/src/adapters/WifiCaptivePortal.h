#pragma once

#include "ports/IPreferences.h"

#include <stdint.h>

namespace feedme::adapters {

// SoftAP + DNS catch-all + HTTP form for Wi-Fi setup.
//
// Two modes:
//
//   Boot (AP only) — first-time setup. Started by main.cpp when prefs
//     has no wifiSsid (and no build-flag fallback). User connects
//     their phone to the AP, fills the form (ssid, password, hid,
//     optional first-user name), and on submit we persist to NVS and
//     reboot. Next boot enters STA mode with the saved creds.
//
//   InPlace (AP + STA) — Switch Wi-Fi while running. STA stays
//     connected to the current network while the SoftAP comes up
//     beside it. User connects from phone, picks a new SSID + types
//     the password, and the device atomically swaps STA without
//     rebooting. Existing feed/snooze state, mood ring, schedule —
//     all preserved.
//
// AP+STA gotcha (in-place mode): both modes share the radio's single
// channel, forced to the current STA channel. When the user submits
// the new creds and STA reconnects on a different channel, the
// SoftAP briefly hops with it — the phone reconnects to the same
// SSID a few seconds later. Acceptable for a one-shot setup.
//
// Lifecycle:
//   begin(prefs)           — boot mode: AP-only, isComplete() flips
//                            to true after form submit; caller reboots.
//   beginInPlace(prefs)    — in-place mode: AP+STA, runs the state
//                            machine below; caller polls state() and
//                            calls stop() after Done/Failed handled.
//   handle()               — pump from loop(); processes DNS + HTTP +
//                            advances the state machine.
//   stop()                 — tear down AP/DNS/HTTP. In-place mode also
//                            drops the AP MAC so we're left with pure
//                            STA on the new network.
class WifiCaptivePortal {
public:
    enum class Mode  { Boot, InPlace };
    enum class State {
        Idle,
        Advertising,   // AP+STA up, waiting for form submit
        Switching,     // dropped old STA, joining new SSID, polling status
        Done,          // STA connected to the new SSID — view should stop()
        Failed,        // 30 s timeout; stays here for retry or cancel
    };

    // Boot mode (AP only). Kept identical to the prior signature so
    // first-time-setup callers don't change.
    void begin(feedme::ports::IPreferences& prefs);

    // In-place mode (AP + STA). Pre-existing STA connection survives.
    void beginInPlace(feedme::ports::IPreferences& prefs);

    // Pump DNS + HTTP and advance the state machine. Call every loop
    // tick while the portal is active.
    void handle();

    // Tear down AP + DNS + HTTP. In-place mode also restores pure STA
    // so the device is left on the (now switched, or unchanged on
    // cancel) network with no SoftAP visible.
    void stop();

    // Boot-mode completion signal — true after a successful POST so
    // main.cpp can reboot. Always false in in-place mode (use state()).
    bool isComplete() const { return complete_; }

    Mode  mode()  const { return mode_; }
    State state() const { return state_; }

    // Latest target SSID the in-place flow is connecting to / connected
    // to. Empty string if the user hasn't submitted the form yet.
    const char* targetSsid() const;

    const char* apName() const { return apName_; }
    const char* apIp()   const { return "192.168.4.1"; }

private:
    feedme::ports::IPreferences* prefs_ = nullptr;
    bool  complete_       = false;
    Mode  mode_           = Mode::Boot;
    State state_          = State::Idle;
    uint32_t switchStartMs_ = 0;   // wall-clock (millis()) of switch begin
    char  apName_[32]     = "feedme-?";
};

}  // namespace feedme::adapters
