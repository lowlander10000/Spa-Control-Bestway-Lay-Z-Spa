#pragma once

#include <Arduino.h>
#include <time.h>

struct TimeStatus {
  bool synchronized = false;
  time_t utc = 0;
  struct tm localTime {};
  unsigned long lastSyncAt = 0;
};

class TimeManager {
public:
  bool begin();
  void loop();

  bool isSynchronized() const;
  time_t nowUtc() const;
  bool getLocalTime(struct tm& out) const;

  String formattedTime() const;
  String formattedDate() const;
  String configuredTimeZone() const;
  const TimeStatus& status() const;

  void forceResync();

private:
  static constexpr const char* DEFAULT_NTP_SERVER_1 = "pool.ntp.org";
  static constexpr const char* DEFAULT_NTP_SERVER_2 = "time.nist.gov";
  static constexpr unsigned long SYNC_CHECK_INTERVAL_MS = 1000;
  static constexpr unsigned long RESYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;

  TimeStatus status_;
  unsigned long lastCheckAt_ = 0;
  unsigned long lastConfigAt_ = 0;
  String activeTimeZone_;
  bool previousWifiConnected_ = false;

  void configureTime();
  void updateStatus();
  bool hasValidTime() const;
  String resolvePosixTimeZone(const String& ianaName) const;
};

extern TimeManager timeManager;
