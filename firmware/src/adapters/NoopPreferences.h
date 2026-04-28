#pragma once

#include "ports/IPreferences.h"

namespace feedme::adapters {

// Headless prefs for the simulator: in-memory only, lost on reboot.
class NoopPreferences : public feedme::ports::IPreferences {
public:
    void begin() override {}
    int64_t getHungryThresholdSec(int64_t defaultValue) override {
        return value_ != 0 ? value_ : defaultValue;
    }
    void setHungryThresholdSec(int64_t value) override { value_ = value; }

private:
    int64_t value_ = 0;
};

}  // namespace feedme::adapters
