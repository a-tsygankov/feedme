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

}  // namespace feedme::adapters
