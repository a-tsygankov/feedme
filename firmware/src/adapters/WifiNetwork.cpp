#include "adapters/WifiNetwork.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

namespace feedme::adapters {

namespace {

constexpr uint32_t HTTP_TIMEOUT_MS = 5000;  // both connect + read

// Drops trailing slashes off the configured base URL so we can build
// "{base}/api/..." without doubling.
std::string normalizeBase(const char* s) {
    std::string out = s ? s : "";
    while (!out.empty() && out.back() == '/') out.pop_back();
    return out;
}

// Builds a per-call WiFiClientSecure. Cheap; HTTPS handshake dominates
// vs object construction.
WiFiClientSecure makeClient() {
    WiFiClientSecure c;
    // No cert verification in v0 — the worker is on Cloudflare's
    // public cert so future setCACert() lands a real chain without
    // API changes here.
    c.setInsecure();
    return c;
}

// Decodes the worker's /api/state JSON into a FeedingState. Schema:
//   { "last": {ts,type,by} | null, "secondsSince": int|null,
//     "todayCount": int, "now": int }
// Maps lastFeedTs from last.ts when type == "feed", snoozeUntilTs
// from last.ts + duration when type == "snooze" (note: server doesn't
// report duration today; we leave snoozeUntilTs at 0 — server-side
// snooze tracking lands with the per-cat extension).
std::optional<feedme::domain::FeedingState> parseStateJson(const String& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) return std::nullopt;

    feedme::domain::FeedingState s{};
    s.todayCount    = doc["todayCount"] | 0;
    s.snoozeUntilTs = 0;  // server doesn't carry this yet

    auto last = doc["last"];
    if (!last.isNull()) {
        const char* type = last["type"] | "";
        if (strcmp(type, "feed") == 0) {
            s.lastFeedTs = last["ts"] | 0;
        }
    }
    return s;
}

}  // namespace

WifiNetwork::WifiNetwork(const char* baseUrl, const char* hid)
    : baseUrl_(normalizeBase(baseUrl)), hid_(hid ? hid : "") {}

void WifiNetwork::setHid(const char* hid) {
    hid_ = hid ? hid : "";
    Serial.printf("[net] WifiNetwork hid -> '%s'\n", hid_.c_str());
}

void WifiNetwork::begin() {
    Serial.printf("[net] WifiNetwork base='%s' hid='%s'\n",
                  baseUrl_.c_str(), hid_.c_str());
}

bool WifiNetwork::isOnline() const {
    return WiFi.status() == WL_CONNECTED;
}

std::string WifiNetwork::ssid() const {
    if (WiFi.status() != WL_CONNECTED) return {};
    return std::string(WiFi.SSID().c_str());
}

int WifiNetwork::rssi() const {
    if (WiFi.status() != WL_CONNECTED) return 0;
    return WiFi.RSSI();
}

std::string WifiNetwork::ipAddress() const {
    if (WiFi.status() != WL_CONNECTED) return {};
    return std::string(WiFi.localIP().toString().c_str());
}

std::optional<feedme::domain::FeedingState>
WifiNetwork::fetchState(uint8_t catId) {
    if (!isOnline() || baseUrl_.empty() || hid_.empty()) return std::nullopt;

    WiFiClientSecure client = makeClient();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    char catBuf[8];
    snprintf(catBuf, sizeof(catBuf), "%u", static_cast<unsigned>(catId));
    std::string url = baseUrl_ + "/api/state?hid=" + hid_ + "&cat=" + catBuf;
    if (tz_) {
        char tzBuf[8];
        snprintf(tzBuf, sizeof(tzBuf), "%d", tz_->offsetMin());
        url += "&tzOffset=";
        url += tzBuf;
    }
    if (!http.begin(client, url.c_str())) return std::nullopt;

    const int code = http.GET();
    std::optional<feedme::domain::FeedingState> out = std::nullopt;
    if (code == 200) {
        out = parseStateJson(http.getString());
    } else {
        Serial.printf("[net] GET /api/state -> %d\n", code);
    }
    http.end();
    return out;
}

bool WifiNetwork::postFeed(const std::string& by, int64_t /*ts*/,
                           uint8_t catId, const std::string& eventId) {
    return postEvent(by, "feed", 0, catId, eventId);
}

bool WifiNetwork::postSnooze(const std::string& by, int64_t /*ts*/,
                             int durationSec, uint8_t catId,
                             const std::string& eventId) {
    return postEvent(by, "snooze", durationSec, catId, eventId);
}

bool WifiNetwork::postEvent(const std::string& by, const char* type,
                            int /*durationSec*/, uint8_t catId,
                            const std::string& eventId) {
    if (!isOnline() || baseUrl_.empty() || hid_.empty()) return false;

    char catBuf[8];
    snprintf(catBuf, sizeof(catBuf), "%u", static_cast<unsigned>(catId));

    JsonDocument doc;
    doc["hid"]  = hid_;
    doc["by"]   = by;
    doc["type"] = type;
    doc["cat"]  = catBuf;
    if (!eventId.empty()) doc["eventId"] = eventId;
    // (Server ignores `note` in this schema; durationSec for snooze
    // would belong here once the schema gains a duration column.)

    String body;
    serializeJson(doc, body);

    WiFiClientSecure client = makeClient();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    const std::string url = baseUrl_ + "/api/feed";
    if (!http.begin(client, url.c_str())) return false;
    http.addHeader("content-type", "application/json");

    const int code = http.POST(body);
    if (code != 200) {
        Serial.printf("[net] POST /api/feed -> %d\n", code);
    }
    http.end();
    return code == 200;
}

}  // namespace feedme::adapters
