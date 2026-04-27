#pragma once

#include "ports/IStorage.h"

namespace feedme::adapters {

class NoopStorage : public feedme::ports::IStorage {
public:
    void begin() override {}
    void enqueue(const feedme::ports::PendingEvent&) override {}
    std::vector<feedme::ports::PendingEvent> drainPending() override { return {}; }
    void recordHistory(const feedme::ports::PendingEvent&) override {}
    std::vector<feedme::ports::PendingEvent> loadRecentHistory(size_t) override { return {}; }
};

}  // namespace feedme::adapters
