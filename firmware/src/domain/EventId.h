#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef ESP_PLATFORM
extern "C" uint32_t esp_random(void);
#endif

namespace feedme::domain {

// 128-bit pseudo-random hex string used as an idempotency key for
// feed/snooze events. Sent on every POST /api/feed and stored in the
// backend's `event_id` column with a UNIQUE constraint — replay of
// the same event (e.g. pending-queue retry after a network timeout
// the server actually processed) gets silently dropped instead of
// double-logging.
//
// Quality: hardware RNG on ESP32 (esp_random uses the radio noise
// source, true random when WiFi/BT is up; pseudo otherwise — still
// fine for collision avoidance at this rate). On host (native tests)
// falls back to std::rand. Caller is responsible for seeding rand()
// once at program start if determinism matters for tests.
inline uint32_t nextRandomU32() {
#ifdef ESP_PLATFORM
    return esp_random();
#else
    return static_cast<uint32_t>(std::rand());
#endif
}

inline std::string generateEventId() {
    char buf[33];
    snprintf(buf, sizeof(buf), "%08lx%08lx%08lx%08lx",
             static_cast<unsigned long>(nextRandomU32()),
             static_cast<unsigned long>(nextRandomU32()),
             static_cast<unsigned long>(nextRandomU32()),
             static_cast<unsigned long>(nextRandomU32()));
    return std::string(buf);
}

}  // namespace feedme::domain
