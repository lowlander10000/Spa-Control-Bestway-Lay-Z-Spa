#pragma once

#include <Arduino.h>
#include "bwc.h"

class SpaInterface {
public:
  void begin();
  void loop();

  bool isConnected() const;
  unsigned long lastPacketTime() const;
  bool hasJets() const;
  String modelName();

  void toggleHeater();
  void toggleFilter();
  void toggleBubbles();
  void toggleJets();

  void setHeater(bool on);
  void setFilter(bool on);
  void setBubbles(bool on);
  void setJets(bool on);
  void setPower(bool on);
  void togglePower();
  void toggleUnit();
  void pressLock();
  void pressTimer();
  void setTargetTemperature(int targetC);
  void changeTarget(int delta);

private:
  BWC bestway_;
  bool connected_ = false;
  unsigned long lastPacketAt_ = 0;
  uint32_t lastGoodPackets_ = 0;

  void syncState();
  void queueCommand(Commands command, int64_t value);
};

extern SpaInterface spaInterface;
