#pragma once
#include <Arduino.h>

struct SpaState {
  bool mqttOnline = false;
  bool heater = false;
  bool heaterActive = false;
  bool filter = false;
  bool bubbles = false;
  bool jets = false;
  bool power = false;
  bool locked = false;
  bool fahrenheit = false;
  bool timerActive = false;

  int temperature = 37;
  int targetTemperature = 39;

  unsigned long bootTime = 0;

  // v45 - live communicatie met de spa
  bool connected = false;
  bool dataValid = false;
  unsigned long lastPacket = 0;

  String uptime();
  String toJson();
  void toggle(String device);
  void changeTarget(int change);

  void setConnectionState(bool online);
  void markPacketReceived();

};

extern SpaState spa;