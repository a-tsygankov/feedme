#include "application/SyncService.h"

#include "adapters/WifiNetwork.h"

#include <Arduino.h>
#include <ArduinoJson.h>

namespace feedme::application {

namespace {
constexpr int  SYNC_SCHEMA_VERSION = 1;
constexpr int  SYNC_DOC_CAPACITY   = 4096;   // safe room for 4 cats + 4 users + home
constexpr char API_PAIR_START[]    = "/api/pair/start";
constexpr char API_PAIR_CHECK[]    = "/api/pair/check?deviceId=";
constexpr char API_PAIR_CANCEL[]   = "/api/pair/cancel";
constexpr char API_SYNC[]          = "/api/sync";

}  // namespace

SyncService::SyncService(feedme::adapters::WifiNetwork& net,
                         feedme::domain::CatRoster& cats,
                         feedme::domain::UserRoster& users)
    : net_(net), cats_(cats), users_(users) {}

// ── Pair lifecycle ───────────────────────────────────────────────

bool SyncService::pairStart() {
    if (deviceId_.empty()) {
        lastError_ = "no deviceId set";
        return false;
    }
    JsonDocument body;
    body["deviceId"] = deviceId_;
    String s; serializeJson(body, s);

    auto res = net_.httpPostJson(API_PAIR_START, std::string(s.c_str()));
    if (res.status != 200) {
        lastError_ = "pairStart status=" + std::to_string(res.status);
        Serial.printf("[sync] pairStart failed: %s\n", lastError_.c_str());
        return false;
    }
    Serial.printf("[sync] pairStart ok deviceId='%s'\n", deviceId_.c_str());
    return true;
}

SyncService::PairResult SyncService::pairCheck() {
    if (cancelRequested_) return PairResult::Cancelled;
    if (deviceId_.empty()) return PairResult::NetworkError;

    const std::string path = std::string(API_PAIR_CHECK) + deviceId_;
    auto res = net_.httpGet(path.c_str());
    if (res.status != 200) {
        Serial.printf("[sync] pairCheck status=%d\n", res.status);
        return PairResult::NetworkError;
    }

    JsonDocument doc;
    if (deserializeJson(doc, res.body) != DeserializationError::Ok) {
        Serial.println("[sync] pairCheck bad JSON");
        return PairResult::NetworkError;
    }
    const char* status = doc["status"] | "";
    if      (strcmp(status, "pending")   == 0) return PairResult::Pending;
    else if (strcmp(status, "expired")   == 0) return PairResult::Expired;
    else if (strcmp(status, "cancelled") == 0) return PairResult::Cancelled;
    else if (strcmp(status, "confirmed") == 0) {
        // Cache the token + home name so the caller can persist them
        // to NVS and the SyncingView can immediately kick syncFull().
        deviceToken_ = doc["token"] | "";
        homeName_    = doc["hid"]   | "";
        if (deviceToken_.empty() || homeName_.empty()) {
            Serial.println("[sync] confirmed but missing token/hid");
            return PairResult::NetworkError;
        }
        Serial.printf("[sync] PAIRED home='%s' token=%d chars\n",
                      homeName_.c_str(), (int)deviceToken_.size());
        return PairResult::Confirmed;
    }
    Serial.printf("[sync] pairCheck unknown status='%s'\n", status);
    return PairResult::UnknownStatus;
}

bool SyncService::pairCancel() {
    if (deviceId_.empty()) return false;
    JsonDocument body;
    body["deviceId"] = deviceId_;
    String s; serializeJson(body, s);
    auto res = net_.httpPostJson(API_PAIR_CANCEL, std::string(s.c_str()));
    Serial.printf("[sync] pairCancel status=%d\n", res.status);
    // Even on failure we treat the cancellation as successful from
    // the device side — UI proceeds back to idle regardless.
    return res.status == 200;
}

// ── Sync ─────────────────────────────────────────────────────────

std::string SyncService::buildSyncRequestBody() {
    JsonDocument doc;
    doc["schemaVersion"] = SYNC_SCHEMA_VERSION;
    doc["deviceId"]      = deviceId_;
    if (lastSyncAt_ > 0) doc["lastSyncAt"] = lastSyncAt_;
    else                 doc["lastSyncAt"] = nullptr;

    auto home = doc["home"].to<JsonObject>();
    home["name"]      = homeName_;
    home["updatedAt"] = static_cast<long long>(0);   // home rename not implemented yet

    auto catsArr = doc["cats"].to<JsonArray>();
    for (int i = 0; i < cats_.count(); ++i) {
        const auto& c = cats_.at(i);
        auto o = catsArr.add<JsonObject>();
        // Phase D: emit uuid only if we have one. Server falls back
        // to (hid, slot_id) lookup when missing — happens for cats
        // added locally that haven't completed a sync yet.
        if (c.uuid[0] != '\0') o["uuid"] = static_cast<const char*>(c.uuid);
        o["slotId"]              = c.id;
        o["name"]                = static_cast<const char*>(c.name);
        o["color"]               = c.avatarColor;
        o["slug"]                = static_cast<const char*>(c.slug);
        o["defaultPortionG"]     = c.portion.grams();
        o["hungryThresholdSec"]  = c.hungryThresholdSec;
        auto sched = o["scheduleHours"].to<JsonArray>();
        for (int s = 0; s < feedme::domain::MealSchedule::SLOT_COUNT; ++s) {
            sched.add(c.schedule.slotHour(s));
        }
        o["createdAt"] = c.createdAt;
        o["updatedAt"] = c.updatedAt;
        o["isDeleted"] = false;
    }

    auto usersArr = doc["users"].to<JsonArray>();
    for (int i = 0; i < users_.count(); ++i) {
        const auto& u = users_.at(i);
        auto o = usersArr.add<JsonObject>();
        if (u.uuid[0] != '\0') o["uuid"] = static_cast<const char*>(u.uuid);
        o["slotId"]    = u.id;
        o["name"]      = static_cast<const char*>(u.name);
        o["color"]     = u.avatarColor;
        o["createdAt"] = u.createdAt;
        o["updatedAt"] = u.updatedAt;
        o["isDeleted"] = false;
    }

    String out; serializeJson(doc, out);
    return std::string(out.c_str());
}

bool SyncService::applySyncResponse(const std::string& body) {
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok) {
        lastError_ = "response not JSON";
        return false;
    }
    if ((doc["schemaVersion"] | 0) != SYNC_SCHEMA_VERSION) {
        lastError_ = "schemaVersion mismatch";
        return false;
    }

    // Replace local rosters with server-canonical state. The server's
    // response includes a uuid for every row (backfilled by migration
    // 0008 for legacy rows; minted at INSERT time for new ones), so
    // by the time we land here the device always learns each cat's
    // canonical identity. Subsequent /api/sync requests then send
    // the uuid back, which lets the server LWW-merge by stable id
    // even across slot_id collisions.
    cats_.clear();
    JsonArray catsArr = doc["cats"];
    for (JsonObject o : catsArr) {
        if (o["isDeleted"] | false) continue;   // skip tombstones (Phase D defers send-side)
        const uint8_t  slotId    = o["slotId"]              | 0;
        const char*    name      = o["name"]                | "Cat";
        const uint32_t color     = o["color"]               | 0;
        const char*    slug      = o["slug"]                | "C2";
        const int      portion   = o["defaultPortionG"]     | 40;
        const int64_t  thresh    = o["hungryThresholdSec"]  | 18000;
        const int64_t  createdAt = o["createdAt"]           | 0;
        const int64_t  updatedAt = o["updatedAt"]           | 0;
        const char*    uuid      = o["uuid"]                | static_cast<const char*>(nullptr);
        cats_.appendLoaded(slotId, name, slug, portion, thresh, color,
                           createdAt, updatedAt, uuid);
    }

    users_.clear();
    JsonArray usersArr = doc["users"];
    for (JsonObject o : usersArr) {
        if (o["isDeleted"] | false) continue;
        const uint8_t  slotId    = o["slotId"]    | 0;
        const char*    name      = o["name"]      | "User";
        const uint32_t color     = o["color"]     | 0;
        const int64_t  createdAt = o["createdAt"] | 0;
        const int64_t  updatedAt = o["updatedAt"] | 0;
        const char*    uuid      = o["uuid"]      | static_cast<const char*>(nullptr);
        users_.appendLoaded(slotId, name, color, createdAt, updatedAt, uuid);
    }

    lastSyncAt_ = doc["now"] | 0;
    // Force-dirty so main.cpp's tick-loop persists the freshly-arrived
    // server state (especially the uuids) to NVS. Without this, the
    // appendLoaded path leaves the rosters "clean" and a reboot would
    // lose the uuids the server just taught us.
    cats_.markDirty();
    users_.markDirty();
    Serial.printf("[sync] applied: %d cats, %d users, lastSyncAt=%lld, conflicts=%d\n",
                  cats_.count(), users_.count(),
                  static_cast<long long>(lastSyncAt_),
                  static_cast<int>(doc["conflicts"] | 0));
    return true;
}

bool SyncService::syncFull() {
    if (deviceToken_.empty()) {
        lastError_ = "not paired (no device token)";
        return false;
    }
    if (cancelRequested_) {
        lastError_ = "cancelled before request";
        return false;
    }

    const std::string body = buildSyncRequestBody();
    auto res = net_.httpPostJson(API_SYNC, body, deviceToken_);

    if (cancelRequested_) {
        lastError_ = "cancelled after request";
        return false;
    }

    if (res.status == 401) {
        lastError_ = "unauthorized — pairing revoked, re-pair from H menu";
        Serial.println("[sync] 401 — clearing local pairing state");
        deviceToken_.clear();
        homeName_.clear();
        return false;
    }
    if (res.status != 200) {
        lastError_ = "sync status=" + std::to_string(res.status);
        Serial.printf("[sync] syncFull failed: %d body=%s\n",
                      res.status, res.body.c_str());
        return false;
    }
    return applySyncResponse(res.body);
}

bool SyncService::unpair() {
    if (deviceToken_.empty() || deviceId_.empty()) return false;
    const std::string path = "/api/pair/" + deviceId_;
    auto res = net_.httpDelete(path.c_str(), deviceToken_);
    Serial.printf("[sync] unpair status=%d\n", res.status);
    // Clear local cache regardless of the network result. Reset's
    // local NVS wipe (caller's responsibility) makes the device
    // unusable for the old home anyway.
    deviceToken_.clear();
    homeName_.clear();
    return res.status == 200 || res.status == 404;   // 404 = already gone, fine
}

}  // namespace feedme::application
