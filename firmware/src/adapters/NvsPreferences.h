#pragma once

#include "ports/IPreferences.h"

#include <Preferences.h>

namespace feedme::adapters {

// NVS-backed preferences. Tiny key/value flash region the ESP32 framework
// already manages — survives reboots, doesn't conflict with the LittleFS
// partition we use for history JSONL.
class NvsPreferences : public feedme::ports::IPreferences {
public:
    void begin() override;
    int64_t getHungryThresholdSec(int64_t defaultValue) override;
    void    setHungryThresholdSec(int64_t value) override;
    int  getPortionGrams(int defaultValue) override;
    void setPortionGrams(int value) override;
    bool getQuietEnabled(bool defaultValue) override;
    void setQuietEnabled(bool value) override;
    int  getWakeHour  (int defaultValue) override;
    int  getWakeMinute(int defaultValue) override;
    void setWakeHour  (int value) override;
    void setWakeMinute(int value) override;
    int  getQuietStartHour  (int defaultValue) override;
    int  getQuietStartMinute(int defaultValue) override;
    int  getQuietEndHour    (int defaultValue) override;
    int  getQuietEndMinute  (int defaultValue) override;
    void setQuietStartHour  (int value) override;
    void setQuietStartMinute(int value) override;
    void setQuietEndHour    (int value) override;
    void setQuietEndMinute  (int value) override;

    int  getCatCount(int defaultValue) override;
    void setCatCount(int value) override;
    int  getCatId   (int slot, int defaultValue) override;
    void setCatId   (int slot, int value) override;
    bool getCatName (int slot, char* buf, int bufLen) override;
    void setCatName (int slot, const char* value) override;
    bool getCatSlug (int slot, char* buf, int bufLen) override;
    void setCatSlug (int slot, const char* value) override;
    int  getCatPortion(int slot, int defaultValue) override;
    void setCatPortion(int slot, int value) override;
    int64_t getCatThresholdSec(int slot, int64_t defaultValue) override;
    void    setCatThresholdSec(int slot, int64_t value) override;
    int     getCatScheduleHour(int catSlot, int mealSlot, int defaultValue) override;
    void    setCatScheduleHour(int catSlot, int mealSlot, int value) override;
    uint32_t getCatColor(int slot, uint32_t defaultValue) override;
    void     setCatColor(int slot, uint32_t value) override;

    int  getTimeZoneOffsetMin(int defaultValue) override;
    void setTimeZoneOffsetMin(int value) override;

    int  getActiveCatIdx(int defaultValue) override;
    void setActiveCatIdx(int value) override;

    int  getSleepTimeoutMin(int defaultValue) override;
    void setSleepTimeoutMin(int value) override;

    int  getLastFeederIdx(int defaultValue) override;
    void setLastFeederIdx(int value) override;

    int  getUserCount(int defaultValue) override;
    void setUserCount(int value) override;
    int  getUserId   (int slot, int defaultValue) override;
    void setUserId   (int slot, int value) override;
    bool getUserName (int slot, char* buf, int bufLen) override;
    void setUserName (int slot, const char* value) override;
    uint32_t getUserColor(int slot, uint32_t defaultValue) override;
    void     setUserColor(int slot, uint32_t value) override;

    bool getWifiSsid(char* buf, int bufLen) override;
    void setWifiSsid(const char* value) override;
    bool getWifiPass(char* buf, int bufLen) override;
    void setWifiPass(const char* value) override;
    bool getHid     (char* buf, int bufLen) override;
    void setHid     (const char* value) override;
    void clearWifiCreds() override;
    bool getPaired(bool defaultValue) override;
    void setPaired(bool value) override;
    int  getHidResetCount(int defaultValue) override;
    void setHidResetCount(int value) override;

    // Phase C sync state.
    int64_t getCatCreatedAt(int slot, int64_t defaultValue) override;
    void    setCatCreatedAt(int slot, int64_t value) override;
    int64_t getCatUpdatedAt(int slot, int64_t defaultValue) override;
    void    setCatUpdatedAt(int slot, int64_t value) override;
    int64_t getUserCreatedAt(int slot, int64_t defaultValue) override;
    void    setUserCreatedAt(int slot, int64_t value) override;
    int64_t getUserUpdatedAt(int slot, int64_t defaultValue) override;
    void    setUserUpdatedAt(int slot, int64_t value) override;
    bool    getDeviceId   (char* buf, int bufLen) override;
    void    setDeviceId   (const char* value) override;
    bool    getDeviceToken(char* buf, int bufLen) override;
    void    setDeviceToken(const char* value) override;
    bool    getCatUuid (int slot, char* buf, int bufLen) override;
    void    setCatUuid (int slot, const char* value) override;
    bool    getUserUuid(int slot, char* buf, int bufLen) override;
    void    setUserUuid(int slot, const char* value) override;
    bool    getHomeName(char* buf, int bufLen) override;
    void    setHomeName(const char* value) override;
    int64_t getLastSyncAt(int64_t defaultValue) override;
    void    setLastSyncAt(int64_t value) override;
    int     getSyncIntervalSec(int defaultValue) override;
    void    setSyncIntervalSec(int value) override;

private:
    ::Preferences prefs_;
    bool          ready_ = false;

    static constexpr const char* NAMESPACE = "feedme";
    static constexpr const char* KEY_HUNGRY_THRESHOLD = "hungryThr";
    static constexpr const char* KEY_PORTION_GRAMS    = "portionG";
    static constexpr const char* KEY_QUIET_ENABLED    = "quietOn";
    static constexpr const char* KEY_WAKE_HOUR        = "wakeH";
    static constexpr const char* KEY_WAKE_MINUTE      = "wakeM";
    static constexpr const char* KEY_QUIET_START_HOUR = "qStartH";
    static constexpr const char* KEY_QUIET_START_MIN  = "qStartM";
    static constexpr const char* KEY_QUIET_END_HOUR   = "qEndH";
    static constexpr const char* KEY_QUIET_END_MIN    = "qEndM";
    // Cat roster — keys are formatted on the fly by the .cpp via
    // catSlotKey(prefix, slot) since NVS keys must be ≤15 chars.
    static constexpr const char* KEY_CAT_COUNT        = "catN";
    static constexpr const char* KEY_USER_COUNT       = "userN";
    static constexpr const char* KEY_TZ_OFFSET_MIN    = "tzMin";
    static constexpr const char* KEY_ACTIVE_CAT_IDX   = "actCat";
    static constexpr const char* KEY_SLEEP_TIMEOUT    = "sleepM";
    static constexpr const char* KEY_LAST_FEEDER_IDX  = "lastFdr";
    // Wi-Fi creds + household id — Phase 2.4 captive portal.
    static constexpr const char* KEY_WIFI_SSID        = "wSsid";
    static constexpr const char* KEY_WIFI_PASS        = "wPass";
    static constexpr const char* KEY_HID              = "hid";
    static constexpr const char* KEY_PAIRED           = "paired";
    static constexpr const char* KEY_HID_RESET_COUNT  = "hidRstN";
    // Phase C sync state.
    static constexpr const char* KEY_DEVICE_ID        = "devId";
    static constexpr const char* KEY_DEVICE_TOKEN     = "devTok";
    static constexpr const char* KEY_HOME_NAME        = "homeNm";
    static constexpr const char* KEY_LAST_SYNC_AT     = "lastSync";
    static constexpr const char* KEY_SYNC_INTERVAL    = "syncIntS";
    // Per-cat / per-user createdAt + updatedAt — keys formatted on
    // the fly via catSlotKey() with prefixes "cCa{slot}" / "cUa{slot}"
    // / "uCa{slot}" / "uUa{slot}". 6-char prefix budget keeps the
    // total ≤ NVS's 15-char limit even with 2-digit slots.
};

}  // namespace feedme::adapters
