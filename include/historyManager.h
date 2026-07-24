#pragma once
#include <Arduino.h>
#include <time.h>

struct HistorySample {
  time_t timestamp = 0;
  int16_t temperature = 0;
  int16_t targetTemperature = 0;
  uint8_t flags = 0;
};

class HistoryManager {
public:
  static constexpr uint16_t MAX_SAMPLES = 288; // 24 uur bij 5 minuten
  bool begin();
  void loop();
  uint16_t count() const;
  const HistorySample* get(uint16_t index) const;
  String toJson(uint16_t maxItems = 288) const;
  bool clear();
private:
  static constexpr const char* HISTORY_FILE = "/history.dat";
  static constexpr unsigned long SAMPLE_INTERVAL_MS = 5UL * 60UL * 1000UL;
  HistorySample samples_[MAX_SAMPLES];
  uint16_t count_ = 0;
  uint16_t writeIndex_ = 0;
  unsigned long lastSampleAt_ = 0;
  bool load();
  bool save();
  void capture();
};
extern HistoryManager historyManager;
