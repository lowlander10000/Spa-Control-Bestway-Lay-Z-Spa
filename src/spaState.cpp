#include "spaState.h"
#include <ESP8266WiFi.h>

SpaState spa;

String SpaState::uptime() {
  unsigned long seconds = (millis() - bootTime) / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;

  seconds %= 60;
  minutes %= 60;

  return String(hours) + "u " + String(minutes) + "m " + String(seconds) + "s";
}

String SpaState::toJson() {
  String ip = WiFi.status() == WL_CONNECTED
    ? WiFi.localIP().toString()
    : WiFi.softAPIP().toString();

  String json = "{";
  json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"mqtt\":" + String(mqttOnline ? "true" : "false") + ",";
  json += "\"dataValid\":" + String(dataValid ? "true" : "false") + ",";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"target\":" + String(targetTemperature) + ",";
  json += "\"heater\":" + String(heater ? "true" : "false") + ",";
  json += "\"heaterActive\":" + String(heaterActive ? "true" : "false") + ",";
  json += "\"filter\":" + String(filter ? "true" : "false") + ",";
  json += "\"bubbles\":" + String(bubbles ? "true" : "false") + ",";
  json += "\"jets\":" + String(jets ? "true" : "false") + ",";
  json += "\"power\":" + String(power ? "true" : "false") + ",";
  json += "\"locked\":" + String(locked ? "true" : "false") + ",";
  json += "\"fahrenheit\":" + String(fahrenheit ? "true" : "false") + ",";
  json += "\"timerActive\":" + String(timerActive ? "true" : "false") + ",";
  json += "\"uptime\":\"" + uptime() + "\",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"chipId\":\"" + String(ESP.getChipId(), HEX) + "\"";
  json += "}";

  return json;
}

void SpaState::toggle(String device) {
  if (device == "heater") heater = !heater;
  if (device == "filter") filter = !filter;
  if (device == "bubbles") bubbles = !bubbles;
  if (device == "jets") jets = !jets;
}

void SpaState::changeTarget(int change) {
  targetTemperature += change;

  if (targetTemperature < 20) targetTemperature = 20;
  if (targetTemperature > 40) targetTemperature = 40;
}

void SpaState::setConnectionState(bool online) {
  connected = online;
}

void SpaState::markPacketReceived() {
  lastPacket = millis();
  dataValid = true;
  connected = true;
}
