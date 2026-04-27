#pragma once

namespace feedme::domain {

enum class Mood {
    Happy,    // < 2h since last feed
    Neutral,  // 2..3h
    Warning,  // 3..4h
    Hungry,   // > 4h (or beyond threshold)
    Fed,      // transient: just logged a feed
    Sleepy,   // transient: snooze active
};

}  // namespace feedme::domain
