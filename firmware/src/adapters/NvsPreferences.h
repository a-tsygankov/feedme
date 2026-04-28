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

private:
    ::Preferences prefs_;
    bool          ready_ = false;

    static constexpr const char* NAMESPACE = "feedme";
    static constexpr const char* KEY_HUNGRY_THRESHOLD = "hungryThr";
};

}  // namespace feedme::adapters
