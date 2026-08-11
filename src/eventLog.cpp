#include "eventLog.h"
#include <LittleFS.h>
#include "timeManager.h"

EventLog eventLog;

bool EventLog::begin() {
  rotateIfNeeded();
  info("Controller gestart");
  return true;
}

void EventLog::info(const String& message) { append("INFO", message); }
void EventLog::warning(const String& message) { append("WARN", message); }
void EventLog::error(const String& message) { append("ERROR", message); }

void EventLog::append(const char* level, const String& message) {
  rotateIfNeeded();
  File file = LittleFS.open(LOG_FILE, "a");
  if (!file) return;

  file.print(static_cast<unsigned long>(timeManager.nowUtc()));
  file.print('|');
  file.print(level);
  file.print('|');
  for (size_t i = 0; i < message.length(); ++i) {
    const char value = message[i];
    file.print((value == '\n' || value == '\r') ? ' ' : value);
  }
  file.println();
  file.close();
}

void EventLog::rotateIfNeeded() {
  if (!LittleFS.exists(LOG_FILE)) return;
  File file = LittleFS.open(LOG_FILE, "r");
  const size_t size = file ? file.size() : 0;
  if (file) file.close();
  if (size > MAX_FILE_SIZE) {
    LittleFS.remove("/events.old.log");
    LittleFS.rename(LOG_FILE, "/events.old.log");
  }
}

String EventLog::toJson(uint16_t maxLines) const {
  File file = LittleFS.open(LOG_FILE, "r");
  if (!file) return "[]";

  const uint16_t limit = constrain(maxLines, 1, 50);
  uint16_t lineCount = 0;
  while (file.available()) {
    if (file.read() == '\n') ++lineCount;
    yield();
  }

  const uint16_t skipLines = lineCount > limit ? lineCount - limit : 0;
  file.seek(0, SeekSet);
  uint16_t skipped = 0;
  while (file.available() && skipped < skipLines) {
    if (file.read() == '\n') ++skipped;
    yield();
  }

  String json;
  json.reserve(256 + static_cast<size_t>(limit) * 96);
  json = '[';
  bool first = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.isEmpty()) continue;

    const int firstSeparator = line.indexOf('|');
    const int secondSeparator = line.indexOf('|', firstSeparator + 1);
    if (firstSeparator < 0 || secondSeparator < 0) continue;

    String message = line.substring(secondSeparator + 1);
    message.replace("\\", "\\\\");
    message.replace("\"", "\\\"");

    if (!first) json += ',';
    first = false;
    json += F("{\"timestamp\":");
    json += line.substring(0, firstSeparator);
    json += F(",\"level\":\"");
    json += line.substring(firstSeparator + 1, secondSeparator);
    json += F("\",\"message\":\"");
    json += message;
    json += F("\"}");
    yield();
  }
  file.close();
  json += ']';
  return json;
}

bool EventLog::clear() {
  if (LittleFS.exists(LOG_FILE)) LittleFS.remove(LOG_FILE);
  if (LittleFS.exists("/events.old.log")) LittleFS.remove("/events.old.log");
  return true;
}
