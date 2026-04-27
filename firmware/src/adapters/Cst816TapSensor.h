#pragma once

#include "ports/ITapSensor.h"

#include <stdint.h>

namespace feedme::adapters {

// CST816D capacitive-touch IC adapter for the CrowPanel 1.28" board.
//
// The CrowPanel's CST816D firmware variant always reports gesture
// code 0x0C regardless of touch duration, so we ignore the chip's
// gesture engine and synthesize SingleTap / DoubleTap / LongPress
// in software by polling finger-count register 0x02 over I²C and
// timing the touches.
//
// Pin map (Elecrow wiki):
//   SDA=6  SCL=7  RST=13  INT=5  (INT not used by the timing path)
class Cst816TapSensor : public feedme::ports::ITapSensor {
public:
    void begin() override;
    void onEvent(Listener listener) override;
    void poll() override;

private:
    void emit(feedme::ports::TapEvent ev);

    Listener  listener_;
    bool      wasTouching_     = false;
    bool      pendingTap_      = false;
    bool      longTouchFired_  = false;
    uint32_t  touchStartMs_    = 0;
    uint32_t  lastTapEndMs_    = 0;
    uint32_t  lastPollMs_      = 0;
};

}  // namespace feedme::adapters
