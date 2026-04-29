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
volatile bool    gComplete = false;
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

void serveForm() {
    String body = kFormHtml;
    String options;
    if (gScannedSsids.empty()) {
        options = "<option value=\"\" disabled selected>(no networks found)</option>";
    } else {
        for (const auto& s : gScannedSsids) {
            options += "<option value=\"" + s + "\">" + s + "</option>";
        }
    }
    body.replace("{SSID_OPTIONS}", options);
    gHttp.send(200, "text/html", body);
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
    // form for any unknown path — simplest reliable approach.
    serveForm();
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

void WifiCaptivePortal::handle() {
    gDns.processNextRequest();
    gHttp.handleClient();

    // Persist + flag complete for main.cpp to reboot on the next tick.
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
}

}  // namespace feedme::adapters
