#pragma once

#include <Arduino.h>

enum class ScheduleAction : uint8_t {
  HeaterOn,
  HeaterOff,
  FilterOn,
  FilterOff,
  BubblesOn,
  BubblesOff,
  JetsOn,
  JetsOff,
  SetTargetTemperature
};

struct ScheduleItem {
  uint16_t id = 0;
  bool enabled = true;

  uint8_t daysMask = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;

  ScheduleAction action = ScheduleAction::HeaterOn;
  int value = 0;

  unsigned long lastRunMinute = 0;
};

class SchedulerManager {
public:
  static constexpr uint8_t MAX_SCHEDULES = 24;

  bool begin();
  void loop();

  bool load();
  bool save();
  bool reset();

  uint8_t count() const;
  const ScheduleItem* get(uint8_t index) const;

  bool add(const ScheduleItem& item);
  bool update(uint16_t id, const ScheduleItem& item);
  bool remove(uint16_t id);
  bool setEnabled(uint16_t id, bool enabled);

private:
  static constexpr const char* SCHEDULE_FILE = "/schedules.dat";

  ScheduleItem schedules_[MAX_SCHEDULES];
  uint8_t scheduleCount_ = 0;
  uint16_t nextId_ = 1;

  void execute(const ScheduleItem& item);
  int findIndexById(uint16_t id) const;
  bool shouldRun(
    const ScheduleItem& item,
    uint8_t weekday,
    uint8_t hour,
    uint8_t minute
  ) const;
};

extern SchedulerManager schedulerManager;
