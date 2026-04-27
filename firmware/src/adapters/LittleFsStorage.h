#pragma once

#include "ports/IStorage.h"

namespace feedme::adapters {

// LittleFS-backed offline queue + history log. Writes JSON-Lines:
//   /pending.jsonl  — offline event queue (drained when network sends)
//   /history.jsonl  — append-only durable log used for the history view
class LittleFsStorage : public feedme::ports::IStorage {
public:
    void begin() override;
    void enqueue(const feedme::ports::PendingEvent& e) override;
    std::vector<feedme::ports::PendingEvent> drainPending() override;
    void recordHistory(const feedme::ports::PendingEvent& e) override;
    std::vector<feedme::ports::PendingEvent> loadRecentHistory(size_t n) override;

private:
    bool ready_ = false;
};

}  // namespace feedme::adapters
