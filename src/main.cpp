#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoOTA.h>

#include "spaState.h"
#include "wifiManager.h"
#include "webApp.h"
#include "spaInterface.h"
#include "mqttManager.h"
#include "settingsManager.h"
#include "schedulerManager.h"
#include "timeManager.h"
#include "historyManager.h"
#include "energyManager.h"
#include "eventLog.h"
#include "configPreserver.h"
#include "maintenanceManager.h"

void setup() {
  Serial.begin(115200);
  delay(500);

  spa.bootTime = millis();

  if (!LittleFS.begin()) {
    Serial.println("LittleFS fout");
    return;
  }

  configPreserverRestoreAfterFilesystemUpdate();

  settingsManager.begin();
  timeManager.begin();
  schedulerManager.begin();
  historyManager.begin();
  energyManager.begin();
  eventLog.begin();
  maintenanceManager.begin();

  wifiBegin();

  ArduinoOTA.setHostname("layzspa-controller");
  ArduinoOTA.begin();

  spaInterface.begin();
  mqttManager.begin();

  webAppBegin();
}

void loop() {
  wifiLoop();
  ArduinoOTA.handle();

  if (webAppFilesystemUpdateActive()) {
    webAppLoop();
    yield();
    return;
  }

  spaInterface.loop();
  mqttManager.loop();
  timeManager.loop();
  schedulerManager.loop();
  historyManager.loop();
  energyManager.loop();
  maintenanceManager.loop();
  webAppLoop();
}
