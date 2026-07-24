#include "historyManager.h"
#include <LittleFS.h>
#include "spaState.h"
#include "timeManager.h"

namespace {
  struct HistoryHeader { uint32_t magic; uint16_t version; uint16_t count; uint16_t writeIndex; };
  constexpr uint32_t MAGIC = 0x48535431;
  constexpr uint16_t VERSION = 1;
  String sampleJson(const HistorySample& s) {
    String json = "{";
    json += "\"timestamp\":" + String((unsigned long)s.timestamp);
    json += ",\"temperature\":" + String(s.temperature);
    json += ",\"target\":" + String(s.targetTemperature);
    json += ",\"heater\":" + String((s.flags & 1) ? "true" : "false");
    json += ",\"filter\":" + String((s.flags & 2) ? "true" : "false");
    json += ",\"bubbles\":" + String((s.flags & 4) ? "true" : "false");
    json += ",\"jets\":" + String((s.flags & 8) ? "true" : "false");
    json += "}";
    return json;
  }
}
HistoryManager historyManager;

bool HistoryManager::begin() {
  count_ = 0; writeIndex_ = 0; lastSampleAt_ = millis();
  if (LittleFS.exists(HISTORY_FILE)) load();
  if (timeManager.isSynchronized()) capture();
  return true;
}
void HistoryManager::loop() {
  if (millis() - lastSampleAt_ < SAMPLE_INTERVAL_MS) return;
  lastSampleAt_ = millis();
  if (timeManager.isSynchronized()) capture();
}
void HistoryManager::capture() {
  HistorySample s;
  s.timestamp = timeManager.nowUtc();
  s.temperature = spa.temperature;
  s.targetTemperature = spa.targetTemperature;
  if (spa.heater) s.flags |= 1;
  if (spa.filter) s.flags |= 2;
  if (spa.bubbles) s.flags |= 4;
  if (spa.jets) s.flags |= 8;
  samples_[writeIndex_] = s;
  writeIndex_ = (writeIndex_ + 1) % MAX_SAMPLES;
  if (count_ < MAX_SAMPLES) count_++;
  save();
}
uint16_t HistoryManager::count() const { return count_; }
const HistorySample* HistoryManager::get(uint16_t index) const {
  if (index >= count_) return nullptr;
  const uint16_t oldest = count_ < MAX_SAMPLES ? 0 : writeIndex_;
  return &samples_[(oldest + index) % MAX_SAMPLES];
}
String HistoryManager::toJson(uint16_t maxItems) const {
  if (maxItems == 0 || maxItems > count_) maxItems = count_;
  const uint16_t start = count_ - maxItems;
  String json = "[";
  for (uint16_t i = start; i < count_; i++) {
    const HistorySample* s = get(i); if (!s) continue;
    if (json.length() > 1) json += ",";
    json += sampleJson(*s);
    yield();
  }
  json += "]";
  return json;
}
bool HistoryManager::clear() {
  count_ = 0; writeIndex_ = 0;
  if (LittleFS.exists(HISTORY_FILE)) LittleFS.remove(HISTORY_FILE);
  return save();
}
bool HistoryManager::load() {
  File f = LittleFS.open(HISTORY_FILE, "r"); if (!f) return false;
  HistoryHeader h{};
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != MAGIC || h.version != VERSION || h.count > MAX_SAMPLES || h.writeIndex >= MAX_SAMPLES) { f.close(); return false; }
  const size_t bytes = sizeof(samples_);
  if (f.read((uint8_t*)samples_, bytes) != bytes) { f.close(); return false; }
  f.close(); count_ = h.count; writeIndex_ = h.writeIndex; return true;
}
bool HistoryManager::save() {
  File f = LittleFS.open(HISTORY_FILE, "w"); if (!f) return false;
  HistoryHeader h{MAGIC, VERSION, count_, writeIndex_};
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h) && f.write((const uint8_t*)samples_, sizeof(samples_)) == sizeof(samples_);
  f.close(); return ok;
}
