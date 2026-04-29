#pragma once

#include "domain/Mood.h"

namespace feedme::ports {

// What the application asks the display to show. Adapters decide HOW.
struct DisplayFrame {
    feedme::domain::Mood mood;
    float ringProgress;   // 0..1
    int   todayCount;     // 0..3 (clamped by display)
    int   minutesSinceFeed;  // for the small label; -1 = never
    int   hour;           // local 0..23 (wall clock from IClock)
    int   minute;         // local 0..59
    char  lastFedBy[16];  // attribution of the most recent feed; empty if none
};

class IDisplay {
public:
    virtual ~IDisplay() = default;

    // Called once after construction, on the LVGL/Arduino thread.
    virtual void begin() = 0;

    // Idempotent re-render. Implementation should diff and skip if unchanged.
    virtual void render(const DisplayFrame& frame) = 0;

    // Pump LVGL / refresh tick. Call from loop() at ~5ms cadence.
    virtual void tick() = 0;
};

}  // namespace feedme::ports
