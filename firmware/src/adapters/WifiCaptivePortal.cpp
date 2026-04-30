#include "adapters/WifiCaptivePortal.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>

#include <vector>

namespace feedme::adapters {

namespace {

// Globals because WebServer's lambdas can't easily capture by reference
// to per-instance state from a header-light adapter. Adequate while we
// only ever instantiate one captive portal.
constexpr int    DNS_PORT = 53;
constexpr int    HTTP_PORT = 80;
DNSServer        gDns;
WebServer        gHttp(HTTP_PORT);
std::vector<String> gScannedSsids;
volatile bool    gComplete = false;          // boot-mode "form submitted"
volatile uint32_t gPendingSwitchAt = 0;       // in-place: deferred-switch wall time
volatile bool    gInPlaceMode = false;        // chooses which form / catch-all
String           gPendingSsid;
String           gPendingPass;
String           gPendingHid;
String           gPendingUser;

// Form HTML. Inline so the binary doesn't need yet another file/asset
// path. Kept readable by avoiding excessive minification — a handful
// of KB makes no difference on a 16 MB flash. The {SSID_OPTIONS}
// placeholder gets replaced server-side with one <option> per scan
// result.
const char kFormHtml[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FeedMe setup</title>
<style>
body{font:16px system-ui;margin:0;padding:24px;background:#1a1226;color:#f6f1e6;}
h1{color:#ffb3c1;font-weight:400;font-size:22px;margin:0 0 18px;}
label{display:block;margin:14px 0 4px;font-size:13px;color:#9c97a4;text-transform:uppercase;letter-spacing:.08em;}
input,select{width:100%;padding:12px;border:1px solid #2e2440;background:#0e0817;color:#f6f1e6;border-radius:6px;font-size:16px;box-sizing:border-box;}
button{margin-top:24px;width:100%;padding:14px;border:0;background:#ffb3c1;color:#1a1226;font-size:16px;font-weight:600;border-radius:6px;cursor:pointer;}
.hint{font-size:12px;color:#9c97a4;margin-top:4px;}
</style></head><body>
<h1>FeedMe setup</h1>
<form method="POST" action="/save">
<label>Wi-Fi network</label>
<select name="ssid" required>{SSID_OPTIONS}</select>
<label>Password</label>
<input type="password" name="pass" autocomplete="off">
<label>Household ID</label>
<input type="text" name="hid" placeholder="home-andrey" required>
<div class="hint">Same string on every device in this home.</div>
<label>Your name (optional)</label>
<input type="text" name="user" placeholder="Andrey">
<div class="hint">Stamps every feed you log. Skip = "User 0".</div>
<button type="submit">Save and connect</button>
</form>
</body></html>)HTML";

const char kDoneHtml[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>body{font:16px system-ui;margin:0;padding:48px;background:#1a1226;color:#f6f1e6;text-align:center;}
h1{color:#ffb3c1;font-weight:400;}</style>
</head><body>
<h1>Saved</h1>
<p>The device is rebooting. You can disconnect from this Wi-Fi.</p>
</body></html>)HTML";

// In-place switch form: SSID dropdown + password only. No hid / user
// inputs (those are already provisioned in the household). Posts to
// /switch instead of /save so the boot and in-place handlers stay
// distinct without per-request mode sniffing.
const char kSwitchFormHtml[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Switch Wi-Fi</title>
<style>
body{font:16px system-ui;margin:0;padding:24px;background:#1a1226;color:#f6f1e6;}
h1{color:#ffb3c1;font-weight:400;font-size:22px;margin:0 0 18px;}
label{display:block;margin:14px 0 4px;font-size:13px;color:#9c97a4;text-transform:uppercase;letter-spacing:.08em;}
input,select{width:100%;padding:12px;border:1px solid #2e2440;background:#0e0817;color:#f6f1e6;border-radius:6px;font-size:16px;box-sizing:border-box;}
button{margin-top:24px;width:100%;padding:14px;border:0;background:#ffb3c1;color:#1a1226;font-size:16px;font-weight:600;border-radius:6px;cursor:pointer;}
.hint{font-size:12px;color:#9c97a4;margin-top:8px;line-height:1.4;}
</style></head><body>
<h1>Switch Wi-Fi</h1>
<form method="POST" action="/switch">
<label>New network</label>
<select name="ssid" required>{SSID_OPTIONS}</select>
<label>Password</label>
<input type="password" name="pass" autocomplete="off">
<button type="submit">Switch</button>
<div class="hint">After you tap Switch, this Wi-Fi (feedme-XXXX) will
briefly drop while the device joins the new network. Reconnect to
feedme-XXXX to see whether it succeeded — or just walk back to the
device; the screen will show the result.</div>
</form>
</body></html>)HTML";

const char kSwitchingHtml[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Switching</title>
<style>body{font:16px system-ui;margin:0;padding:48px;background:#1a1226;color:#f6f1e6;text-align:center;}
h1{color:#ffb3c1;font-weight:400;}</style>
</head><body>
<h1>Switching</h1>
<p>The device is joining the new network. The setup AP will reappear
in a few seconds — reconnect to it if you want to see the result, or
check the screen on the device.</p>
</body></html>)HTML";

String buildSsidOptions() {
    if (gScannedSsids.empty()) {
        return "<option value=\"\" disabled selected>(no networks found)</option>";
    }
    String options;
    for (const auto& s : gScannedSsids) {
        options += "<option value=\"" + s + "\">" + s + "</option>";
    }
    return options;
}

void serveForm() {
    String body = kFormHtml;
    body.replace("{SSID_OPTIONS}", buildSsidOptions());
    gHttp.send(200, "text/html", body);
}

void serveSwitchForm() {
    String body = kSwitchFormHtml;
    body.replace("{SSID_OPTIONS}", buildSsidOptions());
    gHttp.send(200, "text/html", body);
}

void handleSwitch() {
    gPendingSsid = gHttp.arg("ssid");
    gPendingPass = gHttp.arg("pass");
    if (gPendingSsid.isEmpty()) {
        gHttp.send(400, "text/plain", "ssid required");
        return;
    }
    String body = kSwitchingHtml;
    gHttp.send(200, "text/html", body);
    // Defer the actual disconnect/reconnect by 500 ms so the response
    // flushes to the phone before the AP's channel hops with STA.
    gPendingSwitchAt = millis() + 500;
}

void handleSave() {
    gPendingSsid = gHttp.arg("ssid");
    gPendingPass = gHttp.arg("pass");
    gPendingHid  = gHttp.arg("hid");
    gPendingUser = gHttp.arg("user");
    if (gPendingSsid.isEmpty() || gPendingHid.isEmpty()) {
        gHttp.send(400, "text/plain", "ssid and hid required");
        return;
    }
    String body = kDoneHtml;
    gHttp.send(200, "text/html", body);
    gComplete = true;
}

void handleNotFound() {
    // Captive-portal redirect. Most OSes hit a known probe URL; any
    // 200 with the form (or a 302 to it) satisfies "is this an open
    // network?" and triggers the sign-in browser. We just serve the
    // form for any unknown path — simplest reliable approach. Mode
    // selects which form (full first-time vs in-place switch).
    if (gInPlaceMode) serveSwitchForm();
    else              serveForm();
}

}  // namespace

void WifiCaptivePortal::begin(feedme::ports::IPreferences& prefs) {
    prefs_ = &prefs;

    // AP name = feedme-XXXX where XXXX is the last 4 hex chars of the
    // MAC, so each device is distinguishable on a phone's Wi-Fi list.
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apName_, sizeof(apName_), "feedme-%02x%02x", mac[4], mac[5]);

    Serial.printf("[setup] starting captive portal SSID='%s'\n", apName_);

    // Scan first while we're still in pure STA mode — scans are slow
    // (~3 s) but happen once at portal start; results cached for the
    // duration of setup.
    WiFi.mode(WIFI_STA);
    const int n = WiFi.scanNetworks();
    gScannedSsids.clear();
    for (int i = 0; i < n; ++i) {
        gScannedSsids.push_back(WiFi.SSID(i));
    }
    Serial.printf("[setup] scanned %d networks\n", n);

    // Switch to AP mode (open network — no creds needed to join the
    // setup AP itself; the form captures the real Wi-Fi creds).
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName_);
    delay(100);
    const IPAddress ip = WiFi.softAPIP();
    Serial.printf("[setup] AP up, ip=%s\n", ip.toString().c_str());

    gDns.start(DNS_PORT, "*", ip);

    gHttp.on("/",       HTTP_GET,  serveForm);
    gHttp.on("/save",   HTTP_POST, handleSave);
    gHttp.onNotFound(handleNotFound);
    gHttp.begin();

    gComplete = false;
}

void WifiCaptivePortal::beginInPlace(feedme::ports::IPreferences& prefs) {
    prefs_ = &prefs;
    mode_  = Mode::InPlace;
    state_ = State::Advertising;
    complete_ = false;
    gInPlaceMode    = true;
    gPendingSwitchAt = 0;
    gPendingSsid    = "";
    gPendingPass    = "";

    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(apName_, sizeof(apName_), "feedme-%02x%02x", mac[4], mac[5]);

    Serial.printf("[setup] in-place portal SSID='%s' (AP+STA)\n", apName_);

    // Scan first while pure STA — STA is up, no phone has joined an AP
    // yet, so the brief scan disruption hits nobody. ~3 s.
    Serial.println("[setup] scanning networks (in-place)");
    const int n = WiFi.scanNetworks();
    gScannedSsids.clear();
    for (int i = 0; i < n; ++i) {
        gScannedSsids.push_back(WiFi.SSID(i));
    }
    Serial.printf("[setup] scanned %d networks\n", n);

    // Now bring up AP+STA. The SoftAP is forced to whatever channel
    // STA is on; once the user submits new creds and STA reconnects on
    // a (possibly different) channel, the AP follows it. Phone will
    // reconnect to the SoftAP a few seconds later.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apName_);
    delay(100);
    const IPAddress ip = WiFi.softAPIP();
    Serial.printf("[setup] AP up alongside STA, ip=%s\n", ip.toString().c_str());

    gDns.start(DNS_PORT, "*", ip);

    // Register both sets of routes so a single WebServer instance
    // serves either form depending on the active mode. Boot mode's
    // /save handler stays mounted but unreachable — the in-place form
    // posts to /switch.
    gHttp.on("/",        HTTP_GET,  serveSwitchForm);
    gHttp.on("/save",    HTTP_POST, handleSave);
    gHttp.on("/switch",  HTTP_POST, handleSwitch);
    gHttp.onNotFound(handleNotFound);
    gHttp.begin();
}

const char* WifiCaptivePortal::targetSsid() const {
    return gPendingSsid.c_str();
}

void WifiCaptivePortal::handle() {
    gDns.processNextRequest();
    gHttp.handleClient();

    if (mode_ == Mode::Boot) {
        // Boot mode: form submit → flag complete; main.cpp reboots.
        if (gComplete && !complete_) {
            complete_ = true;
            if (prefs_) {
                prefs_->setWifiSsid(gPendingSsid.c_str());
                prefs_->setWifiPass(gPendingPass.c_str());
                prefs_->setHid     (gPendingHid.c_str());
                if (!gPendingUser.isEmpty()) {
                    // Seed user-roster slot 0 with the captured name so
                    // the first feed gets attributed correctly. Roster
                    // load on next boot picks this up.
                    prefs_->setUserCount(1);
                    prefs_->setUserId  (0, 0);
                    prefs_->setUserName(0, gPendingUser.c_str());
                }
                Serial.printf("[setup] saved ssid='%s' hid='%s' user='%s'\n",
                              gPendingSsid.c_str(), gPendingHid.c_str(),
                              gPendingUser.c_str());
            }
        }
        return;
    }

    // In-place mode: state machine.
    switch (state_) {
        case State::Advertising:
            // Form submit set gPendingSwitchAt = now+500ms. Once that
            // window passes, drop old STA and join the new SSID. Doing
            // it here rather than inside handleSwitch lets the HTTP
            // response flush to the phone before the channel hops.
            if (gPendingSwitchAt && millis() >= gPendingSwitchAt) {
                if (prefs_) {
                    prefs_->setWifiSsid(gPendingSsid.c_str());
                    prefs_->setWifiPass(gPendingPass.c_str());
                }
                Serial.printf("[setup] switching STA to '%s'\n",
                              gPendingSsid.c_str());
                // disconnect(false) keeps the AP up — only drops the
                // STA half. Then begin() starts the new STA. Status
                // polling below watches WL_CONNECTED.
                WiFi.disconnect(false);
                delay(50);
                WiFi.begin(gPendingSsid.c_str(), gPendingPass.c_str());
                gPendingSwitchAt = 0;
                switchStartMs_   = millis();
                state_ = State::Switching;
            }
            break;

        case State::Switching: {
            const wl_status_t s = WiFi.status();
            if (s == WL_CONNECTED) {
                Serial.printf("[setup] switch ok, ip=%s rssi=%d\n",
                              WiFi.localIP().toString().c_str(),
                              WiFi.RSSI());
                state_ = State::Done;
            } else if (millis() - switchStartMs_ > 30000) {
                Serial.printf("[setup] switch failed (timeout) status=%d\n",
                              static_cast<int>(s));
                state_ = State::Failed;
            }
            break;
        }

        case State::Done:
        case State::Failed:
        case State::Idle:
            // Terminal-ish states. The owning view watches state() and
            // calls stop() to tear down. Failed state stays here for
            // retry/cancel — the AP is still up and the form still
            // works, so the user can submit different creds.
            break;
    }
}

void WifiCaptivePortal::stop() {
    Serial.println("[setup] portal stopping");
    gHttp.stop();
    gDns.stop();
    if (mode_ == Mode::InPlace) {
        // Drop the AP MAC. STA stays whatever it is (the new network
        // on success, the old network on cancel).
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
    }
    state_ = State::Idle;
    mode_  = Mode::Boot;
    gInPlaceMode    = false;
    gPendingSwitchAt = 0;
    complete_       = false;
}

}  // namespace feedme::adapters
