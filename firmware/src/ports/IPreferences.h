#pragma once

#include <cstdint>

namespace feedme::ports {

// Tiny key/value port for user preferences that should survive reboots.
// Backed by NVS on the real board, no-op in the simulator. Each adapter
// is responsible for its own namespace; the keys here are stable strings.
class IPreferences {
public:
    virtual ~IPreferences() = default;
    virtual void  begin() = 0;

    // hungryThresholdSec — the value DisplayCoordinator clamps and uses.
    virtual int64_t getHungryThresholdSec(int64_t defaultValue) = 0;
    virtual void    setHungryThresholdSec(int64_t value) = 0;

    // portionGrams — default meal size shown on FeedConfirm/PortionAdjust.
    // PortionState clamps the range; adapters just round-trip the int.
    virtual int  getPortionGrams(int defaultValue) = 0;
    virtual void setPortionGrams(int value) = 0;

    // quietEnabled — whether the quiet-hours window is active. The
    // start/end are hardcoded for C.3; persistence of the times is a
    // Phase D follow-up.
    virtual bool getQuietEnabled(bool defaultValue) = 0;
    virtual void setQuietEnabled(bool value) = 0;

    // wakeTime — the household's morning anchor. Minute snaps to
    // 5-minute increments inside WakeTime; adapters round-trip the
    // raw int so on-disk values match what the editor wrote.
    virtual int  getWakeHour  (int defaultValue) = 0;
    virtual int  getWakeMinute(int defaultValue) = 0;
    virtual void setWakeHour  (int value) = 0;
    virtual void setWakeMinute(int value) = 0;

    // quietStart / quietEnd — the QuietWindow time edges. As with
    // WakeTime, the domain object snaps minute to 5-min increments.
    virtual int  getQuietStartHour  (int defaultValue) = 0;
    virtual int  getQuietStartMinute(int defaultValue) = 0;
    virtual int  getQuietEndHour    (int defaultValue) = 0;
    virtual int  getQuietEndMinute  (int defaultValue) = 0;
    virtual void setQuietStartHour  (int value) = 0;
    virtual void setQuietStartMinute(int value) = 0;
    virtual void setQuietEndHour    (int value) = 0;
    virtual void setQuietEndMinute  (int value) = 0;

    // Cat roster — Phase D.5. The firmware only reads/writes per-slot
    // raw fields here; CatRoster does the in-memory bookkeeping.
    // Slot indices 0..N-1 (N = catCount, capped at 4 today).
    virtual int  getCatCount(int defaultValue) = 0;
    virtual void setCatCount(int value) = 0;
    virtual int  getCatId   (int slot, int defaultValue) = 0;
    virtual void setCatId   (int slot, int value) = 0;
    // Strings: caller passes a buffer; adapter copies up to bufLen
    // (incl. NUL). Returns true if a stored value was found.
    virtual bool getCatName (int slot, char* buf, int bufLen) = 0;
    virtual void setCatName (int slot, const char* value) = 0;
    virtual bool getCatSlug (int slot, char* buf, int bufLen) = 0;
    virtual void setCatSlug (int slot, const char* value) = 0;
    // Per-cat portion (grams). Replaces the global portionG key once
    // each cat carries its own. main.cpp seeds slot 0 from the legacy
    // global key on first boot so existing devices don't reset.
    virtual int  getCatPortion(int slot, int defaultValue) = 0;
    virtual void setCatPortion(int slot, int value) = 0;
    // Per-cat hungry threshold (seconds). Replaces the global hungryThr
    // key. Same migration pattern: slot 0 falls back to legacy on
    // first boot.
    virtual int64_t getCatThresholdSec(int slot, int64_t defaultValue) = 0;
    virtual void    setCatThresholdSec(int slot, int64_t value) = 0;
    // Per-cat per-meal-slot schedule hour (0..23). 4 meal slots per
    // cat (Breakfast / Lunch / Dinner / Treat).
    virtual int  getCatScheduleHour(int catSlot, int mealSlot, int defaultValue) = 0;
    virtual void setCatScheduleHour(int catSlot, int mealSlot, int value) = 0;
    // Per-cat avatar color, 0xRRGGBB. 0 = "not stored" → caller
    // defaults to the round-robin Palette assignment.
    virtual uint32_t getCatColor(int slot, uint32_t defaultValue) = 0;
    virtual void     setCatColor(int slot, uint32_t value) = 0;

    // Local timezone offset in minutes (signed). 0 = UTC. Storage
    // unit matches TimeZone::offsetMin to round-trip cleanly.
    virtual int  getTimeZoneOffsetMin(int defaultValue) = 0;
    virtual void setTimeZoneOffsetMin(int value) = 0;

    // Active cat slot — which cat the views currently route their
    // tunables and per-cat state through. Persisted so multi-cat
    // households don't reset to slot 0 every reboot. Range 0..N-1
    // where N = catCount; main.cpp clamps on load if N has shrunk
    // (cat removed) since the last save.
    virtual int  getActiveCatIdx(int defaultValue) = 0;
    virtual void setActiveCatIdx(int value) = 0;

    // User roster — Phase D.6. Same per-slot pattern as cats. There is
    // intentionally no "signed-in user" key here — multiple users may
    // use the same device (per handoff.md § "Entities…", clarified
    // 2026-04-28). Per-feed attribution is captured by an explicit
    // picker in the Feed flow when count > 1; that picker is a
    // follow-up to D.6.
    virtual int  getUserCount(int defaultValue) = 0;
    virtual void setUserCount(int value) = 0;
    virtual int  getUserId   (int slot, int defaultValue) = 0;
    virtual void setUserId   (int slot, int value) = 0;
    virtual bool getUserName (int slot, char* buf, int bufLen) = 0;
    virtual void setUserName (int slot, const char* value) = 0;
    // Per-user avatar color, 0xRRGGBB. Same 0-sentinel semantics as
    // getCatColor.
    virtual uint32_t getUserColor(int slot, uint32_t defaultValue) = 0;
    virtual void     setUserColor(int slot, uint32_t value) = 0;

    // Wi-Fi credentials + household id. Captive portal (Phase 2.4)
    // captures these at first-time setup and stores them here. Empty
    // SSID is the "no creds yet" sentinel — boot path uses it to
    // pick between STA mode and captive-portal setup mode. Strings:
    // caller passes a buffer; adapter copies up to bufLen (incl. NUL).
    // Returns true if a stored value was found.
    virtual bool getWifiSsid(char* buf, int bufLen) = 0;
    virtual void setWifiSsid(const char* value) = 0;
    virtual bool getWifiPass(char* buf, int bufLen) = 0;
    virtual void setWifiPass(const char* value) = 0;
    virtual bool getHid     (char* buf, int bufLen) = 0;
    virtual void setHid     (const char* value) = 0;
    // Drops Wi-Fi creds + hid in one shot — used by Wi-Fi reset to
    // force the next boot into captive-portal setup mode.
    virtual void clearWifiCreds() = 0;
};

}  // namespace feedme::ports
