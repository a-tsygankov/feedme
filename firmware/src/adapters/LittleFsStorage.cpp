#include "adapters/LittleFsStorage.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

namespace feedme::adapters {

namespace {

constexpr const char* PENDING_PATH = "/pending.jsonl";
constexpr const char* HISTORY_PATH = "/history.jsonl";

void writeEventLine(File& f, const feedme::ports::PendingEvent& e) {
    JsonDocument doc;
    doc["ts"]   = e.ts;
    doc["type"] = e.type;
    doc["by"]   = e.by;
    if (!e.clientEventId.empty()) doc["id"] = e.clientEventId;
    serializeJson(doc, f);
    f.write('\n');
}

bool parseEventLine(const String& line, feedme::ports::PendingEvent& out) {
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return false;
    out.ts            = doc["ts"]   | 0;
    out.type          = (const char*)(doc["type"] | "");
    out.by            = (const char*)(doc["by"]   | "");
    out.clientEventId = (const char*)(doc["id"]   | "");
    return true;
}

void appendEvent(const char* path, const feedme::ports::PendingEvent& e) {
    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) return;
    writeEventLine(f, e);
    f.close();
}

}  // namespace

void LittleFsStorage::begin() {
    if (!LittleFS.begin(/*formatOnFail=*/true)) {
        Serial.println("[fs] LittleFS mount FAILED");
        return;
    }
    ready_ = true;
    Serial.printf("[fs] LittleFS mounted, total=%u used=%u\n",
                  LittleFS.totalBytes(), LittleFS.usedBytes());
}

void LittleFsStorage::enqueue(const feedme::ports::PendingEvent& e) {
    if (!ready_) return;
    appendEvent(PENDING_PATH, e);
}

std::vector<feedme::ports::PendingEvent> LittleFsStorage::drainPending() {
    std::vector<feedme::ports::PendingEvent> out;
    if (!ready_) return out;
    if (!LittleFS.exists(PENDING_PATH)) return out;

    File f = LittleFS.open(PENDING_PATH, FILE_READ);
    if (!f) return out;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() == 0) continue;
        feedme::ports::PendingEvent ev;
        if (parseEventLine(line, ev)) out.push_back(std::move(ev));
    }
    f.close();
    LittleFS.remove(PENDING_PATH);  // queue drained — wipe the file
    return out;
}

void LittleFsStorage::recordHistory(const feedme::ports::PendingEvent& e) {
    if (!ready_) return;
    appendEvent(HISTORY_PATH, e);
}

std::vector<feedme::ports::PendingEvent> LittleFsStorage::loadRecentHistory(size_t n) {
    std::vector<feedme::ports::PendingEvent> all;
    if (!ready_) return all;
    if (!LittleFS.exists(HISTORY_PATH)) return all;  // first boot — no log yet

    File f = LittleFS.open(HISTORY_PATH, FILE_READ);
    if (!f) return all;
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.length() == 0) continue;
        feedme::ports::PendingEvent ev;
        if (parseEventLine(line, ev)) all.push_back(std::move(ev));
    }
    f.close();

    // Return last `n` newest-first.
    std::vector<feedme::ports::PendingEvent> out;
    if (all.empty()) return out;
    const size_t take = std::min(n, all.size());
    out.reserve(take);
    for (size_t i = 0; i < take; ++i) {
        out.push_back(all[all.size() - 1 - i]);
    }
    return out;
}

}  // namespace feedme::adapters
