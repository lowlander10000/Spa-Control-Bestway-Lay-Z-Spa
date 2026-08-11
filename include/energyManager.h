#pragma once
#include <Arduino.h>

struct EnergyTotals {
  uint64_t heaterMs = 0;
  uint64_t filterMs = 0;
  uint64_t bubblesMs = 0;
  uint64_t jetsMs = 0;
};

class EnergyManager {
public:
  bool begin();
  void loop();
  const EnergyTotals& totals() const;
  double totalKwh() const;
  double estimatedCost() const;
  String toJson() const;
  bool reset();
  bool restoreTotals(double heaterHours, double filterHours, double bubblesHours, double jetsHours);
private:
  static constexpr const char* ENERGY_FILE = "/energy.dat";
  static constexpr unsigned long SAVE_INTERVAL_MS = 15UL * 60UL * 1000UL;
  EnergyTotals totals_;
  unsigned long lastTickAt_ = 0;
  unsigned long lastSaveAt_ = 0;
  bool load();
  bool save();
};
extern EnergyManager energyManager;
