#pragma once

#include "ports/IPreferences.h"

#include <stdint.h>

namespace feedme::adapters {

// SoftAP + DNS catch-all + HTTP form for first-time Wi-Fi setup.
// Started by main.cpp when prefs has no wifiSsid (and no build-flag
// fallback). User connects their phone to the AP, the captive-portal
// detection kicks in (most OSes auto-open the URL), and they fill the
// form. On submit we persist to NVS and reboot — the next boot enters
// STA mode with the saved creds.
//
// Form fields: ssid (dropdown from a one-shot scan), password,
// household id (optional, defaults to a generated home-XXXX), and an
// optional first-user name (writes to user-roster slot 0).
//
// Lifecycle:
//   begin(prefs, apName)   — starts AP, scans, mounts handlers
//   handle()               — pump from loop(); processes DNS + HTTP
//   isComplete()           — true after a successful POST; main.cpp
//                            reboots on the next loop iteration
//   apName() / apIp()      — strings for the SetupView
class WifiCaptivePortal {
public:
    void begin(feedme::ports::IPreferences& prefs);
    void handle();
    bool isComplete() const { return complete_; }

    const char* apName() const { return apName_; }
    const char* apIp()   const { return "192.168.4.1"; }

private:
    feedme::ports::IPreferences* prefs_ = nullptr;
    bool complete_ = false;
    char apName_[32] = "feedme-?";
};

}  // namespace feedme::adapters
