#pragma once
#include <Arduino.h>
#include <time.h>

struct MaintenanceItem {
  bool enabled = true;
  uint16_t intervalDays = 7;
  time_t lastDone = 0;
};

class MaintenanceManager {
public:
  bool begin();
  bool load();
  bool save();
  void loop();
  String toJson() const;
  bool updateSettings(bool filterReplaceEnabled, uint16_t filterReplaceDays,
                      bool filterCleanEnabled, uint16_t filterCleanDays,
                      bool chlorineEnabled, uint16_t chlorineDays);
  bool markDone(const String& item);
  const MaintenanceItem& filterReplace() const { return filterReplace_; }
  const MaintenanceItem& filterClean() const { return filterClean_; }
  const MaintenanceItem& chlorine() const { return chlorine_; }
  int daysRemaining(const MaintenanceItem& item) const;
  bool due(const MaintenanceItem& item) const;
  String isoDate(time_t value) const;
  String nextDate(const MaintenanceItem& item) const;
  String overallStatus() const;
private:
  MaintenanceItem filterReplace_{true,30,0};
  MaintenanceItem filterClean_{true,7,0};
  MaintenanceItem chlorine_{true,3,0};
  static constexpr const char* FILE_PATH = "/maintenance.json";
};
extern MaintenanceManager maintenanceManager;
