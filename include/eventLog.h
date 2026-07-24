#pragma once
#include <Arduino.h>
class EventLog {
public:
  bool begin();
  void info(const String& message);
  void warning(const String& message);
  void error(const String& message);
  String toJson(uint16_t maxLines = 50) const;
  bool clear();
private:
  static constexpr const char* LOG_FILE = "/events.log";
  static constexpr size_t MAX_FILE_SIZE = 24576;
  void append(const char* level, const String& message);
  void rotateIfNeeded();
};
extern EventLog eventLog;
