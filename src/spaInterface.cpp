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


namespace {
String connectorPinLabel(int gpio) {
  switch (gpio) {
    case 16: return F("D0 / GPIO16");
    case 5: return F("D1 / GPIO5");
    case 4: return F("D2 / GPIO4");
    case 0: return F("D3 / GPIO0");
    case 2: return F("D4 / GPIO2");
    case 14: return F("D5 / GPIO14");
    case 12: return F("D6 / GPIO12");
    case 13: return F("D7 / GPIO13");
    case 15: return F("D8 / GPIO15");
    default: return String(F("GPIO")) + gpio;
  }
}
}

String SpaInterface::runConnectorTest() {
  // Bench connector test, based on the original BWC hardware-test flow:
  // pump disconnected, both spa connectors connected to each other, ESP powered by USB.
  // Stop normal CIO/DSP handling BEFORE touching the bus pins. This prevents the
  // controller itself from being mistaken for live spa traffic during the test.
  bestway_.stop();
  delay(100);
  yield();

  int testPins[6];
  for (int i = 0; i < 6; ++i) testPins[i] = bestway_.pins[i];

  // Give the stopped bus a short settling time, then release all test pins.
  delay(20);
  for (int i = 0; i < 7; ++i) pinMode(bestway_.pins[i], INPUT);

  const char* names[3] = {"data", "clock", "select"};
  int forwardErrors[3] = {0, 0, 0};
  int reverseErrors[3] = {0, 0, 0};
  bool applicable[3] = {true, true, true};
  bool state = false;

  for (int pair = 0; pair < 3; ++pair) {
    const int cioPin = testPins[pair];
    const int dspPin = testPins[pair + 3];
    if (cioPin == dspPin) {
      applicable[pair] = false;
      continue;
    }

    pinMode(cioPin, OUTPUT);
    pinMode(dspPin, INPUT);
    for (int t = 0; t < 100; ++t) {
      state = !state;
      digitalWrite(cioPin, state ? HIGH : LOW);
      delayMicroseconds(100);
      if ((digitalRead(dspPin) == HIGH) != state) ++forwardErrors[pair];
    }
    pinMode(cioPin, INPUT);
    yield();

    pinMode(dspPin, OUTPUT);
    pinMode(cioPin, INPUT);
    for (int t = 0; t < 100; ++t) {
      state = !state;
      digitalWrite(dspPin, state ? HIGH : LOW);
      delayMicroseconds(100);
      if ((digitalRead(cioPin) == HIGH) != state) ++reverseErrors[pair];
    }
    pinMode(dspPin, INPUT);
    yield();
  }

  for (int i = 0; i < 7; ++i) pinMode(bestway_.pins[i], INPUT);

  // Restore normal BWC operation immediately after the short test.
  bestway_.setup();
  bestway_.loop();
  connected_ = false;
  lastPacketAt_ = 0;
  lastGoodPackets_ = 0;
  spa.setConnectionState(false);

  bool overall = true;
  String json;
  json.reserve(768);
  json = F("{\"ok\":true,\"pairs\":[");
  for (int pair = 0; pair < 3; ++pair) {
    if (pair) json += ',';
    const bool pairOk = applicable[pair] && forwardErrors[pair] == 0 && reverseErrors[pair] == 0;
    if (applicable[pair] && !pairOk) overall = false;
    json += F("{\"name\":\""); json += names[pair];
    json += F("\",\"applicable\":"); json += applicable[pair] ? F("true") : F("false");
    json += F(",\"cioPin\":\""); json += connectorPinLabel(testPins[pair]);
    json += F("\",\"dspPin\":\""); json += connectorPinLabel(testPins[pair + 3]);
    json += F("\",\"forwardErrors\":"); json += String(forwardErrors[pair]);
    json += F(",\"reverseErrors\":"); json += String(reverseErrors[pair]);
    json += F(",\"pass\":"); json += pairOk ? F("true") : F("false");
    json += '}';
  }
  json += F("],\"overallPass\":"); json += overall ? F("true") : F("false");
  json += F(",\"powerTested\":false,\"audioTested\":false}");
  return json;
}
