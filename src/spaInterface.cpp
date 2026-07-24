#include "spaInterface.h"
#include "spaState.h"

SpaInterface spaInterface;

namespace {
constexpr int BESTWAY_MIN_TARGET_C = 20;
constexpr int BESTWAY_MAX_TARGET_C = 40;
}

void SpaInterface::begin() {
  bestway_.setup();
  bestway_.loop();

  connected_ = false;
  lastPacketAt_ = 0;
  lastGoodPackets_ = 0;
  spa.setConnectionState(false);

  Serial.println(F("Bestway BWC_unified interface gestart"));
  Serial.print(F("Bestway model: "));
  Serial.println(bestway_.getModel());
}

void SpaInterface::loop() {
  bestway_.loop();

  if (!bestway_.cio) {
    connected_ = false;
    spa.setConnectionState(false);
    return;
  }

  const uint32_t goodPackets = bestway_.cio->good_packets_count;
  if (goodPackets != lastGoodPackets_) {
    lastGoodPackets_ = goodPackets;
    lastPacketAt_ = millis();
    spa.markPacketReceived();
  }

  connected_ = goodPackets > 0 && (millis() - lastPacketAt_ < 10000UL);
  spa.setConnectionState(connected_);

  if (connected_) {
    syncState();
  }
}

bool SpaInterface::isConnected() const {
  return connected_;
}

unsigned long SpaInterface::lastPacketTime() const {
  return lastPacketAt_;
}

bool SpaInterface::hasJets() const {
  return bestway_.hasjets;
}

String SpaInterface::modelName() {
  return bestway_.cio ? bestway_.getModel() : String(F("Bestway"));
}

void SpaInterface::syncState() {
  const sStates &state = bestway_.cio->cio_states;

  // BWC_unified gebruikt: unit = 1 voor Celsius en unit = 0 voor Fahrenheit.
  // De gedeelde SpaState bewaart temperaturen intern in Celsius.
  if (state.unit) {
    spa.temperature = state.temperature;
    spa.targetTemperature = state.target;
  } else {
    spa.temperature = (int)round((state.temperature - 32.0f) * 5.0f / 9.0f);
    spa.targetTemperature = (int)round((state.target - 32.0f) * 5.0f / 9.0f);
  }

  spa.heater = state.heat != 0;
  spa.heaterActive = state.heatred != 0;
  spa.filter = state.pump != 0;
  spa.bubbles = state.bubbles != 0;
  spa.jets = bestway_.hasjets && state.jets != 0;
  spa.power = state.power != 0;
  spa.locked = state.locked != 0;
  spa.fahrenheit = state.unit == 0;
  spa.timerActive = state.timerbuttonled != 0 || state.timerled1 != 0 || state.timerled2 != 0;
}

void SpaInterface::queueCommand(Commands command, int64_t value) {
  command_que_item item;
  item.cmd = command;
  item.val = value;
  item.xtime = 0;
  item.interval = 0;
  bestway_.add_command(item);
}

void SpaInterface::setHeater(bool on) {
  queueCommand(SETHEATER, on ? 1 : 0);
}

void SpaInterface::setFilter(bool on) {
  queueCommand(SETPUMP, on ? 1 : 0);
}

void SpaInterface::setBubbles(bool on) {
  queueCommand(SETBUBBLES, on ? 1 : 0);
}

void SpaInterface::setJets(bool on) {
  if (bestway_.hasjets) {
    queueCommand(SETJETS, on ? 1 : 0);
  }
}

void SpaInterface::setPower(bool on) {
  queueCommand(SETPOWER, on ? 1 : 0);
}

void SpaInterface::togglePower() {
  setPower(!spa.power);
}

void SpaInterface::toggleUnit() {
  // SETUNIT verwacht de BWC-eenheid: 1 = Celsius, 0 = Fahrenheit.
  queueCommand(SETUNIT, spa.fahrenheit ? 1 : 0);
}

void SpaInterface::pressLock() {
  if (bestway_.cio) bestway_.cio->cio_toggles.locked_pressed = true;
}

void SpaInterface::pressTimer() {
  if (bestway_.cio) bestway_.cio->cio_toggles.timer_pressed = true;
}

void SpaInterface::setTargetTemperature(int targetC) {
  targetC = constrain(targetC, BESTWAY_MIN_TARGET_C, BESTWAY_MAX_TARGET_C);

  // BWC_unified accepteert zowel Celsius (1..40) als Fahrenheit (51..104)
  // en zet dit intern om naar de ingestelde eenheid van het bedieningspaneel.
  queueCommand(SETTARGET, targetC);
}

void SpaInterface::toggleHeater() {
  setHeater(!spa.heater);
}

void SpaInterface::toggleFilter() {
  setFilter(!spa.filter);
}

void SpaInterface::toggleBubbles() {
  setBubbles(!spa.bubbles);
}

void SpaInterface::toggleJets() {
  setJets(!spa.jets);
}

void SpaInterface::changeTarget(int delta) {
  if (!bestway_.cio) return;

  // Gedraag je exact als de originele Lay-Z-Spa-knoppen:
  // verhoog/verlaag met 1 in de eenheid die op het bedieningspaneel actief is.
  const sStates &state = bestway_.cio->cio_states;
  int target = static_cast<int>(state.target) + delta;

  if (state.unit) {
    target = constrain(target, 20, 40);     // Celsius
  } else {
    target = constrain(target, 68, 104);    // Fahrenheit
  }

  queueCommand(SETTARGET, target);
}
