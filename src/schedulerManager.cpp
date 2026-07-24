#include "schedulerManager.h"

#include <LittleFS.h>
#include <time.h>

#include "spaInterface.h"
#include "spaState.h"
#include "timeManager.h"

namespace {
  struct ScheduleFileHeader {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint16_t nextId;
  };

  constexpr uint32_t FILE_MAGIC = 0x53434844; // SCHD
  constexpr uint16_t FILE_VERSION = 1;



  uint8_t weekdayToMask(uint8_t weekday) {
    // tm_wday: 0 = zondag, 1 = maandag, ... 6 = zaterdag
    return static_cast<uint8_t>(1U << weekday);
  }
}

SchedulerManager schedulerManager;

bool SchedulerManager::begin() {
  scheduleCount_ = 0;
  nextId_ = 1;

  if (!LittleFS.exists(SCHEDULE_FILE)) {
    Serial.println("Geen schedulerbestand gevonden");
    return save();
  }

  return load();
}

void SchedulerManager::loop() {
  static unsigned long lastCheckAt = 0;

  if (millis() - lastCheckAt < 1000) {
    return;
  }

  lastCheckAt = millis();

  if (!timeManager.isSynchronized()) {
    return;
  }

  struct tm timeInfo;

  if (!timeManager.getLocalTime(timeInfo)) {
    return;
  }

  const time_t now = timeManager.nowUtc();

  const uint8_t weekday =
    static_cast<uint8_t>(timeInfo.tm_wday);

  const uint8_t hour =
    static_cast<uint8_t>(timeInfo.tm_hour);

  const uint8_t minute =
    static_cast<uint8_t>(timeInfo.tm_min);

  const unsigned long currentMinute =
    static_cast<unsigned long>(now / 60);

  for (uint8_t i = 0; i < scheduleCount_; i++) {
    ScheduleItem& item = schedules_[i];

    if (!shouldRun(item, weekday, hour, minute)) {
      continue;
    }

    if (item.lastRunMinute == currentMinute) {
      continue;
    }

    execute(item);
    item.lastRunMinute = currentMinute;
  }
}

bool SchedulerManager::load() {
  File file = LittleFS.open(SCHEDULE_FILE, "r");

  if (!file) {
    Serial.println("Schedulerbestand openen mislukt");
    return false;
  }

  if (file.size() < sizeof(ScheduleFileHeader)) {
    file.close();
    Serial.println("Schedulerbestand ongeldig");
    return false;
  }

  ScheduleFileHeader header;

  if (
    file.read(
      reinterpret_cast<uint8_t*>(&header),
      sizeof(header)
    ) != sizeof(header)
  ) {
    file.close();
    Serial.println("Schedulerheader lezen mislukt");
    return false;
  }

  if (
    header.magic != FILE_MAGIC ||
    header.version != FILE_VERSION ||
    header.count > MAX_SCHEDULES
  ) {
    file.close();
    Serial.println("Schedulerbestand niet compatibel");
    return false;
  }

  const size_t expectedSize =
    sizeof(ScheduleFileHeader) +
    header.count * sizeof(ScheduleItem);

  if (file.size() != expectedSize) {
    file.close();
    Serial.println("Schedulerbestand heeft verkeerde grootte");
    return false;
  }

  scheduleCount_ =
    static_cast<uint8_t>(header.count);

  nextId_ = header.nextId > 0
    ? header.nextId
    : 1;

  for (uint8_t i = 0; i < scheduleCount_; i++) {
    if (
      file.read(
        reinterpret_cast<uint8_t*>(&schedules_[i]),
        sizeof(ScheduleItem)
      ) != sizeof(ScheduleItem)
    ) {
      file.close();
      scheduleCount_ = 0;
      nextId_ = 1;
      Serial.println("Scheduleritem lezen mislukt");
      return false;
    }

    schedules_[i].lastRunMinute = 0;
  }

  file.close();

  Serial.print("Scheduler geladen: ");
  Serial.print(scheduleCount_);
  Serial.println(" schema's");

  return true;
}

bool SchedulerManager::save() {
  File file = LittleFS.open(SCHEDULE_FILE, "w");

  if (!file) {
    Serial.println("Schedulerbestand schrijven mislukt");
    return false;
  }

  const ScheduleFileHeader header = {
    FILE_MAGIC,
    FILE_VERSION,
    scheduleCount_,
    nextId_
  };

  if (
    file.write(
      reinterpret_cast<const uint8_t*>(&header),
      sizeof(header)
    ) != sizeof(header)
  ) {
    file.close();
    Serial.println("Schedulerheader schrijven mislukt");
    return false;
  }

  for (uint8_t i = 0; i < scheduleCount_; i++) {
    ScheduleItem storedItem = schedules_[i];
    storedItem.lastRunMinute = 0;

    if (
      file.write(
        reinterpret_cast<const uint8_t*>(&storedItem),
        sizeof(ScheduleItem)
      ) != sizeof(ScheduleItem)
    ) {
      file.close();
      Serial.println("Scheduleritem schrijven mislukt");
      return false;
    }
  }

  file.close();
  Serial.println("Scheduler opgeslagen");
  return true;
}

bool SchedulerManager::reset() {
  scheduleCount_ = 0;
  nextId_ = 1;

  if (LittleFS.exists(SCHEDULE_FILE)) {
    LittleFS.remove(SCHEDULE_FILE);
  }

  return save();
}

uint8_t SchedulerManager::count() const {
  return scheduleCount_;
}

const ScheduleItem* SchedulerManager::get(
  uint8_t index
) const {
  if (index >= scheduleCount_) {
    return nullptr;
  }

  return &schedules_[index];
}

bool SchedulerManager::add(
  const ScheduleItem& item
) {
  if (scheduleCount_ >= MAX_SCHEDULES) {
    return false;
  }

  ScheduleItem newItem = item;
  newItem.id = nextId_++;
  newItem.lastRunMinute = 0;

  schedules_[scheduleCount_] = newItem;
  scheduleCount_++;

  return save();
}

bool SchedulerManager::update(
  uint16_t id,
  const ScheduleItem& item
) {
  const int index = findIndexById(id);

  if (index < 0) {
    return false;
  }

  ScheduleItem updated = item;
  updated.id = id;
  updated.lastRunMinute = 0;

  schedules_[index] = updated;
  return save();
}

bool SchedulerManager::remove(uint16_t id) {
  const int index = findIndexById(id);

  if (index < 0) {
    return false;
  }

  for (
    uint8_t i = static_cast<uint8_t>(index);
    i + 1 < scheduleCount_;
    i++
  ) {
    schedules_[i] = schedules_[i + 1];
  }

  scheduleCount_--;
  return save();
}

bool SchedulerManager::setEnabled(
  uint16_t id,
  bool enabled
) {
  const int index = findIndexById(id);

  if (index < 0) {
    return false;
  }

  schedules_[index].enabled = enabled;
  return save();
}

void SchedulerManager::execute(
  const ScheduleItem& item
) {
  switch (item.action) {
    case ScheduleAction::HeaterOn:
      spaInterface.setHeater(true);
      break;

    case ScheduleAction::HeaterOff:
      spaInterface.setHeater(false);
      break;

    case ScheduleAction::FilterOn:
      spaInterface.setFilter(true);
      break;

    case ScheduleAction::FilterOff:
      spaInterface.setFilter(false);
      break;

    case ScheduleAction::BubblesOn:
      spaInterface.setBubbles(true);
      break;

    case ScheduleAction::BubblesOff:
      spaInterface.setBubbles(false);
      break;

    case ScheduleAction::JetsOn:
      spaInterface.setJets(true);
      break;

    case ScheduleAction::JetsOff:
      spaInterface.setJets(false);
      break;

    case ScheduleAction::SetTargetTemperature: {
      int target = item.value;

      if (target < 20) {
        target = 20;
      }

      if (target > 40) {
        target = 40;
      }

      spaInterface.setTargetTemperature(target);
      break;
    }
  }

  Serial.print("Scheduler uitgevoerd, id=");
  Serial.println(item.id);
}

int SchedulerManager::findIndexById(
  uint16_t id
) const {
  for (uint8_t i = 0; i < scheduleCount_; i++) {
    if (schedules_[i].id == id) {
      return i;
    }
  }

  return -1;
}

bool SchedulerManager::shouldRun(
  const ScheduleItem& item,
  uint8_t weekday,
  uint8_t hour,
  uint8_t minute
) const {
  if (!item.enabled) {
    return false;
  }

  if (item.daysMask == 0) {
    return false;
  }

  if (
    (item.daysMask & weekdayToMask(weekday)) == 0
  ) {
    return false;
  }

  return
    item.hour == hour &&
    item.minute == minute;
}
