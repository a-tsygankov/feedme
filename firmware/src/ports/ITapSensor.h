#pragma once

#include <functional>

namespace feedme::ports {

enum class TapEvent {
    SingleTap,   // log a feed
    LongPress,   // snooze
    DoubleTap,   // history
};

class ITapSensor {
public:
    using Listener = std::function<void(TapEvent)>;

    virtual ~ITapSensor() = default;
    virtual void begin() = 0;
    virtual void onEvent(Listener listener) = 0;
    virtual void poll() = 0;  // call from loop()
};

}  // namespace feedme::ports
