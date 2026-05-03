#include "adapters/NvsPreferences.h"

#include <Arduino.h>

namespace feedme::adapters {

void NvsPreferences::begin() {
    // RW namespace; auto-creates on first use.
    if (!prefs_.begin(NAMESPACE, /*readOnly=*/false)) {
        Serial.println("[prefs] NVS open FAILED");
        return;
    }
    ready_ = true;
    Serial.println("[prefs] NVS namespace 'feedme' open");
}

int64_t NvsPreferences::getHungryThresholdSec(int64_t defaultValue) {
    if (!ready_) return defaultValue;
    // Preferences API takes int64_t directly via getLong64.
    return prefs_.getLong64(KEY_HUNGRY_THRESHOLD, defaultValue);
}

void NvsPreferences::setHungryThresholdSec(int64_t value) {
    if (!ready_) return;
    prefs_.putLong64(KEY_HUNGRY_THRESHOLD, value);
}

int NvsPreferences::getPortionGrams(int defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getInt(KEY_PORTION_GRAMS, defaultValue);
}

void NvsPreferences::setPortionGrams(int value) {
    if (!ready_) return;
    prefs_.putInt(KEY_PORTION_GRAMS, value);
}

bool NvsPreferences::getQuietEnabled(bool defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getBool(KEY_QUIET_ENABLED, defaultValue);
}

void NvsPreferences::setQuietEnabled(bool value) {
    if (!ready_) return;
    prefs_.putBool(KEY_QUIET_ENABLED, value);
}

int NvsPreferences::getWakeHour(int defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getInt(KEY_WAKE_HOUR, defaultValue);
}

int NvsPreferences::getWakeMinute(int defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getInt(KEY_WAKE_MINUTE, defaultValue);
}

void NvsPreferences::setWakeHour(int value) {
    if (!ready_) return;
    prefs_.putInt(KEY_WAKE_HOUR, value);
}

void NvsPreferences::setWakeMinute(int value) {
    if (!ready_) return;
    prefs_.putInt(KEY_WAKE_MINUTE, value);
}

int NvsPreferences::getQuietStartHour(int d)   { return ready_ ? prefs_.getInt(KEY_QUIET_START_HOUR, d) : d; }
int NvsPreferences::getQuietStartMinute(int d) { return ready_ ? prefs_.getInt(KEY_QUIET_START_MIN,  d) : d; }
int NvsPreferences::getQuietEndHour(int d)     { return ready_ ? prefs_.getInt(KEY_QUIET_END_HOUR,   d) : d; }
int NvsPreferences::getQuietEndMinute(int d)   { return ready_ ? prefs_.getInt(KEY_QUIET_END_MIN,    d) : d; }

void NvsPreferences::setQuietStartHour(int v)   { if (ready_) prefs_.putInt(KEY_QUIET_START_HOUR, v); }
void NvsPreferences::setQuietStartMinute(int v) { if (ready_) prefs_.putInt(KEY_QUIET_START_MIN,  v); }
void NvsPreferences::setQuietEndHour(int v)     { if (ready_) prefs_.putInt(KEY_QUIET_END_HOUR,   v); }
void NvsPreferences::setQuietEndMinute(int v)   { if (ready_) prefs_.putInt(KEY_QUIET_END_MIN,    v); }

namespace {
// NVS keys must be ≤15 chars. "catSlugN" / "catNameN" / "catIdN" all fit
// for slot N in 0..9.
void formatKey(char* out, int outLen, const char* prefix, int slot) {
    snprintf(out, outLen, "%s%d", prefix, slot);
}
}  // namespace

int  NvsPreferences::getCatCount(int d)            { return ready_ ? prefs_.getInt(KEY_CAT_COUNT, d) : d; }
void NvsPreferences::setCatCount(int v)            { if (ready_) prefs_.putInt(KEY_CAT_COUNT, v); }

int  NvsPreferences::getCatId(int slot, int d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "catId",   slot);
    return prefs_.getInt(k, d);
}
void NvsPreferences::setCatId(int slot, int v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "catId",   slot);
    prefs_.putInt(k, v);
}
bool NvsPreferences::getCatName(int slot, char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    char k[12]; formatKey(k, sizeof(k), "catName", slot);
    const size_t n = prefs_.getString(k, buf, bufLen);
    return n > 0;
}
void NvsPreferences::setCatName(int slot, const char* value) {
    if (!ready_ || !value) return;
    char k[12]; formatKey(k, sizeof(k), "catName", slot);
    prefs_.putString(k, value);
}
bool NvsPreferences::getCatSlug(int slot, char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    char k[12]; formatKey(k, sizeof(k), "catSlug", slot);
    const size_t n = prefs_.getString(k, buf, bufLen);
    return n > 0;
}
void NvsPreferences::setCatSlug(int slot, const char* value) {
    if (!ready_ || !value) return;
    char k[12]; formatKey(k, sizeof(k), "catSlug", slot);
    prefs_.putString(k, value);
}
int  NvsPreferences::getCatPortion(int slot, int d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "catPort", slot);
    return prefs_.getInt(k, d);
}
void NvsPreferences::setCatPortion(int slot, int v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "catPort", slot);
    prefs_.putInt(k, v);
}
int64_t NvsPreferences::getCatThresholdSec(int slot, int64_t d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "catThr",  slot);
    return prefs_.getLong64(k, d);
}
void NvsPreferences::setCatThresholdSec(int slot, int64_t v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "catThr",  slot);
    prefs_.putLong64(k, v);
}

namespace {
// "csch{cat}_{meal}" — fits in NVS's 15-char key limit (max 9 chars).
void formatScheduleKey(char* out, int outLen, int catSlot, int mealSlot) {
    snprintf(out, outLen, "csch%d_%d", catSlot, mealSlot);
}
}  // namespace

int NvsPreferences::getCatScheduleHour(int c, int m, int d) {
    if (!ready_) return d;
    char k[12]; formatScheduleKey(k, sizeof(k), c, m);
    return prefs_.getInt(k, d);
}
void NvsPreferences::setCatScheduleHour(int c, int m, int v) {
    if (!ready_) return;
    char k[12]; formatScheduleKey(k, sizeof(k), c, m);
    prefs_.putInt(k, v);
}

uint32_t NvsPreferences::getCatColor(int slot, uint32_t d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "catCol",  slot);
    return prefs_.getUInt(k, d);
}
void NvsPreferences::setCatColor(int slot, uint32_t v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "catCol",  slot);
    prefs_.putUInt(k, v);
}

int  NvsPreferences::getTimeZoneOffsetMin(int d) { return ready_ ? prefs_.getInt(KEY_TZ_OFFSET_MIN, d) : d; }
void NvsPreferences::setTimeZoneOffsetMin(int v) { if (ready_) prefs_.putInt(KEY_TZ_OFFSET_MIN, v); }

int  NvsPreferences::getActiveCatIdx(int d) { return ready_ ? prefs_.getInt(KEY_ACTIVE_CAT_IDX, d) : d; }
void NvsPreferences::setActiveCatIdx(int v) { if (ready_) prefs_.putInt(KEY_ACTIVE_CAT_IDX, v); }

int  NvsPreferences::getSleepTimeoutMin(int d) { return ready_ ? prefs_.getInt(KEY_SLEEP_TIMEOUT, d) : d; }
void NvsPreferences::setSleepTimeoutMin(int v) { if (ready_) prefs_.putInt(KEY_SLEEP_TIMEOUT, v); }

int  NvsPreferences::getLastFeederIdx(int d) { return ready_ ? prefs_.getInt(KEY_LAST_FEEDER_IDX, d) : d; }
void NvsPreferences::setLastFeederIdx(int v) { if (ready_) prefs_.putInt(KEY_LAST_FEEDER_IDX, v); }

int  NvsPreferences::getUserCount(int d) { return ready_ ? prefs_.getInt(KEY_USER_COUNT, d) : d; }
void NvsPreferences::setUserCount(int v) { if (ready_) prefs_.putInt(KEY_USER_COUNT, v); }

int  NvsPreferences::getUserId(int slot, int d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "uId",   slot);
    return prefs_.getInt(k, d);
}
void NvsPreferences::setUserId(int slot, int v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "uId",   slot);
    prefs_.putInt(k, v);
}
bool NvsPreferences::getUserName(int slot, char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    char k[12]; formatKey(k, sizeof(k), "uName", slot);
    return prefs_.getString(k, buf, bufLen) > 0;
}
void NvsPreferences::setUserName(int slot, const char* value) {
    if (!ready_ || !value) return;
    char k[12]; formatKey(k, sizeof(k), "uName", slot);
    prefs_.putString(k, value);
}

uint32_t NvsPreferences::getUserColor(int slot, uint32_t d) {
    if (!ready_) return d;
    char k[12]; formatKey(k, sizeof(k), "uCol", slot);
    return prefs_.getUInt(k, d);
}
void NvsPreferences::setUserColor(int slot, uint32_t v) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "uCol", slot);
    prefs_.putUInt(k, v);
}

bool NvsPreferences::getWifiSsid(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_WIFI_SSID, buf, bufLen) > 0;
}
void NvsPreferences::setWifiSsid(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_WIFI_SSID, value);
}
bool NvsPreferences::getWifiPass(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_WIFI_PASS, buf, bufLen) > 0;
}
void NvsPreferences::setWifiPass(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_WIFI_PASS, value);
}
bool NvsPreferences::getHid(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_HID, buf, bufLen) > 0;
}
void NvsPreferences::setHid(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_HID, value);
}
void NvsPreferences::clearWifiCreds() {
    if (!ready_) return;
    prefs_.remove(KEY_WIFI_SSID);
    prefs_.remove(KEY_WIFI_PASS);
    prefs_.remove(KEY_HID);
    // Also clear the paired flag so the user sees the pairing screen
    // again on the next boot (new hid will be generated, new QR shown).
    prefs_.remove(KEY_PAIRED);
}

bool NvsPreferences::getPaired(bool defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getBool(KEY_PAIRED, defaultValue);
}

void NvsPreferences::setPaired(bool value) {
    if (!ready_) return;
    prefs_.putBool(KEY_PAIRED, value);
}

int NvsPreferences::getHidResetCount(int defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getInt(KEY_HID_RESET_COUNT, defaultValue);
}

void NvsPreferences::setHidResetCount(int value) {
    if (!ready_) return;
    prefs_.putInt(KEY_HID_RESET_COUNT, value);
}

// ── Phase C sync state ─────────────────────────────────────────
//
// Per-cat / per-user createdAt + updatedAt — keys formatted on the
// fly via formatKey() with prefixes that fit the NVS 15-char cap
// even with a 2-digit slot. We keep prefixes tight: "cCa" (Cat
// CreatedAt), "cUa" (Cat UpdatedAt), "uCa" (User CreatedAt),
// "uUa" (User UpdatedAt) → e.g. "cCa12" = 5 chars.

int64_t NvsPreferences::getCatCreatedAt(int slot, int64_t defaultValue) {
    if (!ready_) return defaultValue;
    char k[12]; formatKey(k, sizeof(k), "cCa", slot);
    return prefs_.getLong64(k, defaultValue);
}
void NvsPreferences::setCatCreatedAt(int slot, int64_t value) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "cCa", slot);
    prefs_.putLong64(k, value);
}
int64_t NvsPreferences::getCatUpdatedAt(int slot, int64_t defaultValue) {
    if (!ready_) return defaultValue;
    char k[12]; formatKey(k, sizeof(k), "cUa", slot);
    return prefs_.getLong64(k, defaultValue);
}
void NvsPreferences::setCatUpdatedAt(int slot, int64_t value) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "cUa", slot);
    prefs_.putLong64(k, value);
}
int64_t NvsPreferences::getUserCreatedAt(int slot, int64_t defaultValue) {
    if (!ready_) return defaultValue;
    char k[12]; formatKey(k, sizeof(k), "uCa", slot);
    return prefs_.getLong64(k, defaultValue);
}
void NvsPreferences::setUserCreatedAt(int slot, int64_t value) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "uCa", slot);
    prefs_.putLong64(k, value);
}
int64_t NvsPreferences::getUserUpdatedAt(int slot, int64_t defaultValue) {
    if (!ready_) return defaultValue;
    char k[12]; formatKey(k, sizeof(k), "uUa", slot);
    return prefs_.getLong64(k, defaultValue);
}
void NvsPreferences::setUserUpdatedAt(int slot, int64_t value) {
    if (!ready_) return;
    char k[12]; formatKey(k, sizeof(k), "uUa", slot);
    prefs_.putLong64(k, value);
}

// Device identity + token — written by SyncService after a
// successful pairing handshake; read on every /api/sync. The token
// can be ~200 chars (HMAC base64url); putString handles up to
// ~4000-byte values which is plenty.
bool NvsPreferences::getDeviceId(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_DEVICE_ID, buf, bufLen) > 0;
}
void NvsPreferences::setDeviceId(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_DEVICE_ID, value);
}
bool NvsPreferences::getDeviceToken(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_DEVICE_TOKEN, buf, bufLen) > 0;
}
void NvsPreferences::setDeviceToken(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_DEVICE_TOKEN, value);
}

bool NvsPreferences::getHomeName(char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    return prefs_.getString(KEY_HOME_NAME, buf, bufLen) > 0;
}
void NvsPreferences::setHomeName(const char* value) {
    if (!ready_ || !value) return;
    prefs_.putString(KEY_HOME_NAME, value);
}

int64_t NvsPreferences::getLastSyncAt(int64_t defaultValue) {
    if (!ready_) return defaultValue;
    return prefs_.getLong64(KEY_LAST_SYNC_AT, defaultValue);
}
void NvsPreferences::setLastSyncAt(int64_t value) {
    if (!ready_) return;
    prefs_.putLong64(KEY_LAST_SYNC_AT, value);
}

// ── Phase D: per-entity UUIDs ───────────────────────────────────
//
// Key prefixes "cUu{slot}" / "uUu{slot}" — 5 chars max, well under
// NVS's 15-char limit. Empty string is "no uuid yet" — legacy NVS
// rows fall through to here with a 0-length string until the first
// sync populates them.

bool NvsPreferences::getCatUuid(int slot, char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    char k[12]; formatKey(k, sizeof(k), "cUu", slot);
    return prefs_.getString(k, buf, bufLen) > 0;
}
void NvsPreferences::setCatUuid(int slot, const char* value) {
    if (!ready_ || !value) return;
    char k[12]; formatKey(k, sizeof(k), "cUu", slot);
    prefs_.putString(k, value);
}
bool NvsPreferences::getUserUuid(int slot, char* buf, int bufLen) {
    if (!ready_ || !buf || bufLen <= 0) return false;
    char k[12]; formatKey(k, sizeof(k), "uUu", slot);
    return prefs_.getString(k, buf, bufLen) > 0;
}
void NvsPreferences::setUserUuid(int slot, const char* value) {
    if (!ready_ || !value) return;
    char k[12]; formatKey(k, sizeof(k), "uUu", slot);
    prefs_.putString(k, value);
}

}  // namespace feedme::adapters
