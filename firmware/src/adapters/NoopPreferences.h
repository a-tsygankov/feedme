#pragma once

#include "ports/IPreferences.h"

namespace feedme::adapters {

// Headless prefs for the simulator: in-memory only, lost on reboot.
class NoopPreferences : public feedme::ports::IPreferences {
public:
    void begin() override {}
    int64_t getHungryThresholdSec(int64_t defaultValue) override {
        return threshold_ != 0 ? threshold_ : defaultValue;
    }
    void setHungryThresholdSec(int64_t value) override { threshold_ = value; }

    int  getPortionGrams(int defaultValue) override {
        return portion_ != 0 ? portion_ : defaultValue;
    }
    void setPortionGrams(int value) override { portion_ = value; }

    bool getQuietEnabled(bool defaultValue) override {
        return quietHasValue_ ? quietEnabled_ : defaultValue;
    }
    void setQuietEnabled(bool value) override {
        quietEnabled_ = value;
        quietHasValue_ = true;
    }

    int  getWakeHour  (int defaultValue) override {
        return wakeHasValue_ ? wakeHour_ : defaultValue;
    }
    int  getWakeMinute(int defaultValue) override {
        return wakeHasValue_ ? wakeMinute_ : defaultValue;
    }
    void setWakeHour(int value) override {
        wakeHour_ = value;
        wakeHasValue_ = true;
    }
    void setWakeMinute(int value) override {
        wakeMinute_ = value;
        wakeHasValue_ = true;
    }

    int  getQuietStartHour  (int d) override { return qHasValue_ ? qStartH_ : d; }
    int  getQuietStartMinute(int d) override { return qHasValue_ ? qStartM_ : d; }
    int  getQuietEndHour    (int d) override { return qHasValue_ ? qEndH_   : d; }
    int  getQuietEndMinute  (int d) override { return qHasValue_ ? qEndM_   : d; }
    void setQuietStartHour  (int v) override { qStartH_ = v; qHasValue_ = true; }
    void setQuietStartMinute(int v) override { qStartM_ = v; qHasValue_ = true; }
    void setQuietEndHour    (int v) override { qEndH_   = v; qHasValue_ = true; }
    void setQuietEndMinute  (int v) override { qEndM_   = v; qHasValue_ = true; }

    // Cats — minimal in-RAM placeholder so the simulator can run the
    // editor without a backing store. Loses everything on reboot.
    int  getCatCount(int d) override { return catCountSet_ ? catCount_ : d; }
    void setCatCount(int v) override { catCount_ = v; catCountSet_ = true; }
    int  getCatId(int slot, int d) override {
        return (slot >= 0 && slot < 4 && catIdSet_[slot]) ? catId_[slot] : d;
    }
    void setCatId(int slot, int v) override {
        if (slot < 0 || slot >= 4) return;
        catId_[slot] = v;
        catIdSet_[slot] = true;
    }
    bool getCatName(int slot, char* buf, int bufLen) override {
        if (slot < 0 || slot >= 4 || !catNameSet_[slot] || !buf) return false;
        // Minimal copy — sim tests only.
        const char* src = catName_[slot];
        int i = 0;
        while (src[i] && i < bufLen - 1) { buf[i] = src[i]; ++i; }
        buf[i] = '\0';
        return true;
    }
    void setCatName(int slot, const char* v) override {
        if (slot < 0 || slot >= 4 || !v) return;
        int i = 0;
        while (v[i] && i < 15) { catName_[slot][i] = v[i]; ++i; }
        catName_[slot][i] = '\0';
        catNameSet_[slot] = true;
    }
    bool getCatSlug(int slot, char* buf, int bufLen) override {
        if (slot < 0 || slot >= 4 || !catSlugSet_[slot] || !buf) return false;
        const char* src = catSlug_[slot];
        int i = 0;
        while (src[i] && i < bufLen - 1) { buf[i] = src[i]; ++i; }
        buf[i] = '\0';
        return true;
    }
    void setCatSlug(int slot, const char* v) override {
        if (slot < 0 || slot >= 4 || !v) return;
        int i = 0;
        while (v[i] && i < 3) { catSlug_[slot][i] = v[i]; ++i; }
        catSlug_[slot][i] = '\0';
        catSlugSet_[slot] = true;
    }
    int  getCatPortion(int slot, int d) override {
        return (slot >= 0 && slot < 4 && catPortionSet_[slot]) ? catPortion_[slot] : d;
    }
    void setCatPortion(int slot, int v) override {
        if (slot < 0 || slot >= 4) return;
        catPortion_[slot] = v;
        catPortionSet_[slot] = true;
    }
    int64_t getCatThresholdSec(int slot, int64_t d) override {
        return (slot >= 0 && slot < 4 && catThreshSet_[slot]) ? catThresh_[slot] : d;
    }
    void setCatThresholdSec(int slot, int64_t v) override {
        if (slot < 0 || slot >= 4) return;
        catThresh_[slot] = v;
        catThreshSet_[slot] = true;
    }
    int  getCatScheduleHour(int c, int m, int d) override {
        return (c >= 0 && c < 4 && m >= 0 && m < 4 && catSchedSet_[c][m])
               ? catSched_[c][m] : d;
    }
    void setCatScheduleHour(int c, int m, int v) override {
        if (c < 0 || c >= 4 || m < 0 || m >= 4) return;
        catSched_[c][m]    = v;
        catSchedSet_[c][m] = true;
    }
    uint32_t getCatColor(int slot, uint32_t d) override {
        return (slot >= 0 && slot < 4 && catColorSet_[slot]) ? catColor_[slot] : d;
    }
    void setCatColor(int slot, uint32_t v) override {
        if (slot < 0 || slot >= 4) return;
        catColor_[slot] = v;
        catColorSet_[slot] = true;
    }
    int  getTimeZoneOffsetMin(int d) override { return tzHasValue_ ? tzMin_ : d; }
    void setTimeZoneOffsetMin(int v) override { tzMin_ = v; tzHasValue_ = true; }

    int  getActiveCatIdx(int d) override { return activeCatHasValue_ ? activeCatIdx_ : d; }
    void setActiveCatIdx(int v) override { activeCatIdx_ = v; activeCatHasValue_ = true; }

    int  getSleepTimeoutMin(int d) override { return sleepHasValue_ ? sleepMin_ : d; }
    void setSleepTimeoutMin(int v) override { sleepMin_ = v; sleepHasValue_ = true; }

    int  getLastFeederIdx(int d) override { return lastFeederHasValue_ ? lastFeederIdx_ : d; }
    void setLastFeederIdx(int v) override { lastFeederIdx_ = v; lastFeederHasValue_ = true; }

    int  getUserCount(int d) override { return userCountSet_ ? userCount_ : d; }
    void setUserCount(int v) override { userCount_ = v; userCountSet_ = true; }
    int  getUserId(int slot, int d) override {
        return (slot >= 0 && slot < 4 && userIdSet_[slot]) ? userId_[slot] : d;
    }
    void setUserId(int slot, int v) override {
        if (slot < 0 || slot >= 4) return;
        userId_[slot] = v;
        userIdSet_[slot] = true;
    }
    bool getUserName(int slot, char* buf, int bufLen) override {
        if (slot < 0 || slot >= 4 || !userNameSet_[slot] || !buf) return false;
        const char* src = userName_[slot];
        int i = 0;
        while (src[i] && i < bufLen - 1) { buf[i] = src[i]; ++i; }
        buf[i] = '\0';
        return true;
    }
    void setUserName(int slot, const char* v) override {
        if (slot < 0 || slot >= 4 || !v) return;
        int i = 0;
        while (v[i] && i < 15) { userName_[slot][i] = v[i]; ++i; }
        userName_[slot][i] = '\0';
        userNameSet_[slot] = true;
    }
    uint32_t getUserColor(int slot, uint32_t d) override {
        return (slot >= 0 && slot < 4 && userColorSet_[slot]) ? userColor_[slot] : d;
    }
    void setUserColor(int slot, uint32_t v) override {
        if (slot < 0 || slot >= 4) return;
        userColor_[slot] = v;
        userColorSet_[slot] = true;
    }

    // Wi-Fi creds — sim is never online so always "not set". Captive
    // portal logic on real hardware uses the NvsPreferences impl.
    bool getWifiSsid(char*, int) override { return false; }
    void setWifiSsid(const char*) override {}
    bool getWifiPass(char*, int) override { return false; }
    void setWifiPass(const char*) override {}
    bool getHid     (char*, int) override { return false; }
    void setHid     (const char*) override {}
    void clearWifiCreds() override {}
    bool getPaired(bool d) override { return pairedHasValue_ ? paired_ : d; }
    void setPaired(bool v) override { paired_ = v; pairedHasValue_ = true; }
    int  getHidResetCount(int d) override { return hidResetHasValue_ ? hidReset_ : d; }
    void setHidResetCount(int v) override { hidReset_ = v; hidResetHasValue_ = true; }

    // Phase C sync state — sim is offline so most paths are no-ops,
    // but we provide in-RAM storage so sync-flow unit tests can run.
    int64_t getCatCreatedAt(int slot, int64_t d) override {
        return (slot >= 0 && slot < 4 && catCreatedAtSet_[slot]) ? catCreatedAt_[slot] : d;
    }
    void setCatCreatedAt(int slot, int64_t v) override {
        if (slot < 0 || slot >= 4) return;
        catCreatedAt_[slot] = v; catCreatedAtSet_[slot] = true;
    }
    int64_t getCatUpdatedAt(int slot, int64_t d) override {
        return (slot >= 0 && slot < 4 && catUpdatedAtSet_[slot]) ? catUpdatedAt_[slot] : d;
    }
    void setCatUpdatedAt(int slot, int64_t v) override {
        if (slot < 0 || slot >= 4) return;
        catUpdatedAt_[slot] = v; catUpdatedAtSet_[slot] = true;
    }
    int64_t getUserCreatedAt(int slot, int64_t d) override {
        return (slot >= 0 && slot < 4 && userCreatedAtSet_[slot]) ? userCreatedAt_[slot] : d;
    }
    void setUserCreatedAt(int slot, int64_t v) override {
        if (slot < 0 || slot >= 4) return;
        userCreatedAt_[slot] = v; userCreatedAtSet_[slot] = true;
    }
    int64_t getUserUpdatedAt(int slot, int64_t d) override {
        return (slot >= 0 && slot < 4 && userUpdatedAtSet_[slot]) ? userUpdatedAt_[slot] : d;
    }
    void setUserUpdatedAt(int slot, int64_t v) override {
        if (slot < 0 || slot >= 4) return;
        userUpdatedAt_[slot] = v; userUpdatedAtSet_[slot] = true;
    }
    bool getDeviceId(char*, int) override     { return false; }
    void setDeviceId(const char*) override    {}
    bool getDeviceToken(char*, int) override  { return false; }
    void setDeviceToken(const char*) override {}
    bool getHomeName(char*, int) override     { return false; }
    void setHomeName(const char*) override    {}
    int64_t getLastSyncAt(int64_t d) override { return lastSyncSet_ ? lastSync_ : d; }
    void setLastSyncAt(int64_t v) override    { lastSync_ = v; lastSyncSet_ = true; }

private:
    int64_t threshold_     = 0;
    int     portion_       = 0;
    bool    quietEnabled_  = false;
    bool    quietHasValue_ = false;
    int     wakeHour_      = 0;
    int     wakeMinute_    = 0;
    bool    wakeHasValue_  = false;
    int     qStartH_       = 0;
    int     qStartM_       = 0;
    int     qEndH_         = 0;
    int     qEndM_         = 0;
    bool    qHasValue_     = false;
    int     catCount_      = 0;
    bool    catCountSet_   = false;
    int     catId_  [4]    = {0};
    bool    catIdSet_[4]   = {false};
    char    catName_[4][16]= {{0}};
    bool    catNameSet_[4] = {false};
    char    catSlug_[4][4] = {{0}};
    bool    catSlugSet_[4] = {false};
    int     userCount_     = 0;
    bool    userCountSet_  = false;
    int     userId_  [4]   = {0};
    bool    userIdSet_[4]  = {false};
    char    userName_[4][16] = {{0}};
    bool    userNameSet_[4] = {false};
    int     catPortion_[4] = {0};
    bool    catPortionSet_[4] = {false};
    int64_t catThresh_[4] = {0};
    bool    catThreshSet_[4] = {false};
    int     catSched_   [4][4] = {{0}};
    bool    catSchedSet_[4][4] = {{false}};
    int     tzMin_       = 0;
    bool    tzHasValue_  = false;
    int     activeCatIdx_      = 0;
    bool    activeCatHasValue_ = false;
    uint32_t catColor_   [4]   = {0};
    bool     catColorSet_[4]   = {false};
    uint32_t userColor_  [4]   = {0};
    bool     userColorSet_[4]  = {false};
    int      sleepMin_         = 0;
    bool     sleepHasValue_    = false;
    int      lastFeederIdx_    = 0;
    bool     lastFeederHasValue_ = false;
    bool     paired_           = false;
    bool     pairedHasValue_   = false;
    int      hidReset_         = 0;
    bool     hidResetHasValue_ = false;
    int64_t  catCreatedAt_   [4] = {0};
    bool     catCreatedAtSet_[4] = {false};
    int64_t  catUpdatedAt_   [4] = {0};
    bool     catUpdatedAtSet_[4] = {false};
    int64_t  userCreatedAt_  [4] = {0};
    bool     userCreatedAtSet_[4]= {false};
    int64_t  userUpdatedAt_  [4] = {0};
    bool     userUpdatedAtSet_[4]= {false};
    int64_t  lastSync_         = 0;
    bool     lastSyncSet_      = false;
};

}  // namespace feedme::adapters
