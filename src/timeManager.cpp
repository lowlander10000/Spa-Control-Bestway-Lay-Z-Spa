#include "timeManager.h"

#include <ESP8266WiFi.h>

#include "settingsManager.h"

namespace {
  constexpr time_t MIN_VALID_EPOCH = 1700000000;

  String twoDigits(int value) {
    return value < 10 ? "0" + String(value) : String(value);
  }
}

TimeManager timeManager;

bool TimeManager::begin() {
  status_ = TimeStatus{};
  lastCheckAt_ = 0;
  lastConfigAt_ = 0;
  activeTimeZone_ = "";
  previousWifiConnected_ = WiFi.status() == WL_CONNECTED;

  if (previousWifiConnected_) configureTime();
  updateStatus();
  return true;
}

void TimeManager::loop() {
  const unsigned long nowMs = millis();
  if (nowMs - lastCheckAt_ < SYNC_CHECK_INTERVAL_MS) return;
  lastCheckAt_ = nowMs;

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  const String configured = settingsManager.regional().timeZone;

  if (wifiConnected && !previousWifiConnected_) {
    configureTime();
  } else if (wifiConnected && configured != activeTimeZone_) {
    configureTime();
  } else if (
    wifiConnected &&
    nowMs - lastConfigAt_ >= RESYNC_INTERVAL_MS
  ) {
    configureTime();
  }

  previousWifiConnected_ = wifiConnected;
  updateStatus();
}

bool TimeManager::isSynchronized() const { return status_.synchronized; }
time_t TimeManager::nowUtc() const { return time(nullptr); }

bool TimeManager::getLocalTime(struct tm& out) const {
  if (!hasValidTime()) return false;
  const time_t now = time(nullptr);
  return localtime_r(&now, &out) != nullptr;
}

String TimeManager::formattedTime() const {
  struct tm local;
  if (!getLocalTime(local)) return "--:--";

  if (settingsManager.regional().use24HourClock) {
    return twoDigits(local.tm_hour) + ":" + twoDigits(local.tm_min);
  }

  int hour = local.tm_hour % 12;
  if (hour == 0) hour = 12;
  return twoDigits(hour) + ":" + twoDigits(local.tm_min) +
    (local.tm_hour < 12 ? " AM" : " PM");
}

String TimeManager::formattedDate() const {
  struct tm local;
  if (!getLocalTime(local)) return "----/--/--";

  const String day = twoDigits(local.tm_mday);
  const String month = twoDigits(local.tm_mon + 1);
  const String year = String(local.tm_year + 1900);
  const String format = settingsManager.regional().dateFormat;

  if (format == "MM/DD/YYYY") return month + "/" + day + "/" + year;
  if (format == "YYYY-MM-DD") return year + "-" + month + "-" + day;
  return day + "-" + month + "-" + year;
}

String TimeManager::configuredTimeZone() const {
  return settingsManager.regional().timeZone;
}

const TimeStatus& TimeManager::status() const { return status_; }

void TimeManager::forceResync() {
  lastConfigAt_ = 0;
  if (WiFi.status() == WL_CONNECTED) configureTime();
}

void TimeManager::configureTime() {
  if (WiFi.status() != WL_CONNECTED) return;

  String ianaTimeZone = settingsManager.regional().timeZone;
  ianaTimeZone.trim();
  if (ianaTimeZone.isEmpty()) ianaTimeZone = "UTC";

  const String posixRule = resolvePosixTimeZone(ianaTimeZone);

  configTzTime(
    posixRule.c_str(),
    DEFAULT_NTP_SERVER_1,
    DEFAULT_NTP_SERVER_2
  );

  activeTimeZone_ = ianaTimeZone;
  lastConfigAt_ = millis();

  Serial.print("Tijdzone ingesteld: ");
  Serial.print(activeTimeZone_);
  Serial.print(" -> ");
  Serial.println(posixRule);
}

void TimeManager::updateStatus() {
  const bool wasSynchronized = status_.synchronized;
  status_.utc = time(nullptr);
  status_.synchronized = hasValidTime();

  if (!status_.synchronized) return;

  if (localtime_r(&status_.utc, &status_.localTime) == nullptr) {
    status_.synchronized = false;
    return;
  }

  if (!wasSynchronized) {
    status_.lastSyncAt = millis();
    Serial.print("Tijd gesynchroniseerd: ");
    Serial.print(formattedDate());
    Serial.print(" ");
    Serial.println(formattedTime());
  }
}

bool TimeManager::hasValidTime() const {
  return time(nullptr) >= MIN_VALID_EPOCH;
}

String TimeManager::resolvePosixTimeZone(const String& ianaName) const {
  // Uitbreidbare vertaallaag: gebruikers bewaren IANA-namen,
  // de ESP8266 ontvangt POSIX TZ-regels.
  if (ianaName == "Europe/Amsterdam" || ianaName == "Europe/Berlin" ||
      ianaName == "Europe/Paris" || ianaName == "Europe/Brussels") {
    return "CET-1CEST,M3.5.0,M10.5.0/3";
  }
  if (ianaName == "Europe/London") {
    return "GMT0BST,M3.5.0/1,M10.5.0";
  }
  if (ianaName == "America/New_York") {
    return "EST5EDT,M3.2.0/2,M11.1.0/2";
  }
  if (ianaName == "America/Chicago") {
    return "CST6CDT,M3.2.0/2,M11.1.0/2";
  }
  if (ianaName == "America/Denver") {
    return "MST7MDT,M3.2.0/2,M11.1.0/2";
  }
  if (ianaName == "America/Los_Angeles") {
    return "PST8PDT,M3.2.0/2,M11.1.0/2";
  }
  if (ianaName == "Australia/Sydney") {
    return "AEST-10AEDT,M10.1.0,M4.1.0/3";
  }
  if (ianaName == "Asia/Tokyo") return "JST-9";
  if (ianaName == "Asia/Singapore") return "SGT-8";
  if (ianaName == "UTC" || ianaName == "Etc/UTC") return "UTC0";

  Serial.print("Onbekende IANA-tijdzone, UTC gebruikt: ");
  Serial.println(ianaName);
  return "UTC0";
}
