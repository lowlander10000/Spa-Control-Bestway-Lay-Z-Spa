#include "wifiManager.h"
#include "webApp.h"
#include "spaState.h"
#include "spaInterface.h"
#include "settingsManager.h"
#include "schedulerManager.h"
#include "timeManager.h"
#include "historyManager.h"
#include "energyManager.h"
#include "eventLog.h"
#include "mqttManager.h"
#include "configPreserver.h"

#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Updater.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

unsigned long lastBroadcast = 0;

bool otaUploadSucceeded = false;
bool otaFilesystemUpload = false;
bool otaRestartPending = false;
unsigned long otaRestartAt = 0;

String escapeJson(String value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  value.replace("\n", "\\n");
  value.replace("\r", "\\r");
  return value;
}

void handleRoot() {
  File file = LittleFS.open("/index.html", "r");

  if (!file) {
    server.send(404, "text/plain", "index.html niet gevonden");
    return;
  }

  server.streamFile(file, "text/html");
  file.close();
}

void handleStatus() {
  server.send(200, "application/json", spa.toJson());
}

void webAppBroadcast() {
  String json = spa.toJson();
  webSocket.broadcastTXT(json);
}

void processCommand(String msg) {
  if (msg == "toggle:heater") spaInterface.toggleHeater();
  else if (msg == "toggle:filter") spaInterface.toggleFilter();
  else if (msg == "toggle:bubbles") spaInterface.toggleBubbles();
  else if (msg == "toggle:jets") spaInterface.toggleJets();
  else if (msg == "toggle:power") spaInterface.togglePower();
  else if (msg == "toggle:unit") spaInterface.toggleUnit();
  else if (msg == "press:lock") spaInterface.pressLock();
  else if (msg == "press:timer") spaInterface.pressTimer();
  else if (msg == "target:+") spaInterface.changeTarget(1);
  else if (msg == "target:-") spaInterface.changeTarget(-1);

  webAppBroadcast();
}

void webSocketEvent(
  uint8_t num,
  WStype_t type,
  uint8_t* payload,
  size_t length
) {
  if (type == WStype_CONNECTED) {
    Serial.println("WebSocket client verbonden");

    String json = spa.toJson();
    webSocket.sendTXT(num, json);
  }

  if (type == WStype_TEXT) {
    String msg;
    msg.reserve(length);

    for (size_t i = 0; i < length; i++) {
      msg += static_cast<char>(payload[i]);
    }

    processCommand(msg);
  }

  if (type == WStype_DISCONNECTED) {
    Serial.println("WebSocket client losgekoppeld");
  }
}


void handleHardwareGet() {
  File file = LittleFS.open("/bestway_hwcfg.json", "r");
  if (!file) {
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"Hardwareconfiguratie niet gevonden\"}");
    return;
  }
  server.streamFile(file, "application/json");
  file.close();
}

void handleHardwarePost() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Lege aanvraag\"}");
    return;
  }

  const String body = server.arg("plain");
  DynamicJsonDocument requestDoc(1024);
  const DeserializationError requestError = deserializeJson(requestDoc, body);
  if (requestError) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Ongeldige JSON\"}");
    return;
  }

  const int cio = requestDoc["cio"] | -1;
  const int dsp = requestDoc["dsp"] | -1;
  const char* pcb = requestDoc["pcb"] | "";
  JsonArray pins = requestDoc["pins"].as<JsonArray>();
  if (cio < 0 || cio > 8 || dsp < 0 || dsp > 8 || !pins || pins.size() != 8 ||
      !(String(pcb) == "v1" || String(pcb) == "v2" || String(pcb) == "v2b" || String(pcb) == "custom")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Ongeldige hardware-instellingen\"}");
    return;
  }

  for (JsonVariant pin : pins) {
    const int value = pin.as<int>();
    if (value < 0 || value > 16) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"GPIO moet tussen 0 en 16 liggen\"}");
      return;
    }
  }

  // Start with the existing file so settings managed elsewhere, such as
  // pwr_levels from the Energy page, are never removed by the Hardware page.
  DynamicJsonDocument mergedDoc(2048);
  File existingFile = LittleFS.open("/bestway_hwcfg.json", "r");
  if (existingFile) {
    deserializeJson(mergedDoc, existingFile);
    existingFile.close();
  }

  mergedDoc["cio"] = cio;
  mergedDoc["dsp"] = dsp;
  mergedDoc["pcb"] = pcb;
  mergedDoc["hasTempSensor"] = requestDoc["hasTempSensor"] | "0";
  mergedDoc.remove("pins");
  JsonArray mergedPins = mergedDoc.createNestedArray("pins");
  for (JsonVariant pin : pins) mergedPins.add(pin.as<int>());

  File file = LittleFS.open("/bestway_hwcfg.json", "w");
  if (!file) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Opslaan mislukt\"}");
    return;
  }
  serializeJsonPretty(mergedDoc, file);
  file.close();
  eventLog.info("Hardwareconfiguratie opgeslagen; herstart vereist");
  server.send(200, "application/json", "{\"ok\":true,\"restartRequired\":true}");
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting");
  delay(250);
  ESP.restart();
}

void handleWifiScan() {
  const WifiScanState state = wifiGetScanState();

  if (state == WifiScanState::Idle) {
    if (!wifiStartScan()) {
      server.send(
        500,
        "application/json",
        "{\"status\":\"failed\",\"error\":\"Scan kon niet starten\"}"
      );
      return;
    }

    server.send(
      202,
      "application/json",
      "{\"status\":\"scanning\"}"
    );
    return;
  }

  if (state == WifiScanState::Scanning) {
    server.send(
      202,
      "application/json",
      "{\"status\":\"scanning\"}"
    );
    return;
  }

  if (state == WifiScanState::Failed) {
    String response = "{\"status\":\"failed\",\"error\":\"";
    response += escapeJson(wifiLastError());
    response += "\"}";

    wifiClearScanResults();
    server.send(500, "application/json", response);
    return;
  }

  String result = wifiGetScanResultsJson();
  wifiClearScanResults();
  server.send(200, "application/json", result);
}

void handleWifiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(
      400,
      "application/json",
      "{\"ok\":false,\"status\":\"failed\",\"error\":\"SSID ontbreekt\"}"
    );
    return;
  }

  const String ssid = server.arg("ssid");
  const String password = server.arg("password");
  const bool useDhcp = !server.hasArg("mode") || server.arg("mode") != "static";
  const String ip = server.arg("ip");
  const String gateway = server.arg("gateway");
  const String subnet = server.arg("subnet");
  const String dns1 = server.arg("dns1");
  const String dns2 = server.arg("dns2");

  if (!wifiConnect(ssid, password, useDhcp, ip, gateway, subnet, dns1, dns2)) {
    String response = "{\"ok\":false,\"status\":\"failed\",\"error\":\"";
    response += escapeJson(wifiLastError());
    response += "\"}";

    server.send(400, "application/json", response);
    return;
  }

  server.send(
    202,
    "application/json",
    "{\"ok\":true,\"status\":\"connecting\"}"
  );
}

void handleWifiForget() {
  bool ok = wifiForgetCredentials();

  server.send(
    ok ? 200 : 500,
    "application/json",
    ok ? "{\"ok\":true}" : "{\"ok\":false}"
  );
}

void handleWifiStatus() {
    server.send(
        200,
        "application/json",
        wifiGetStatusJson()
    );
}


void finishOtaResponse(const char* typeName) {
  if (Update.hasError() || !otaUploadSucceeded) {
    if (otaFilesystemUpload) {
      LittleFS.begin();
    }
    String error = String("{\"ok\":false,\"error\":\"") + typeName + "-update mislukt\"}";
    server.send(500, "application/json", error);
    return;
  }

  String message = String("{\"ok\":true,\"message\":\"") + typeName +
                   "-update voltooid. ESP start opnieuw op.\"}";
  server.send(200, "application/json", message);
  otaRestartPending = true;
  otaRestartAt = millis() + 1200;
}

void processOtaUpload(bool filesystem) {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaUploadSucceeded = false;
    otaFilesystemUpload = filesystem;

    Serial.print(filesystem ? "LittleFS upload gestart: " : "Firmware upload gestart: ");
    Serial.println(upload.filename);

    bool started = false;
    if (filesystem) {
      // U_FS mag niet met grootte 0 worden gestart. Gebruik de volledige
      // LittleFS-partitiegrootte als maximale OTA-schrijfomvang.
      FSInfo fsInfo;
      const bool fsInfoOk = LittleFS.info(fsInfo);
      const size_t filesystemSize = fsInfoOk ? fsInfo.totalBytes : 0;
      const bool configBackupOk =
        fsInfoOk && filesystemSize > 0 &&
        configPreserverBackupForFilesystemUpdate();

      if (configBackupOk) LittleFS.end();
      started = configBackupOk && Update.begin(filesystemSize, U_FS);

      if (!fsInfoOk || filesystemSize == 0) {
        Serial.println("LittleFS partitiegrootte kon niet worden bepaald");
      } else if (!configBackupOk) {
        Serial.println("LittleFS-update afgebroken: configuratieback-up mislukt");
      }
    } else {
      const uint32_t maxSketchSpace =
        (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      started = Update.begin(maxSketchSpace, U_FLASH);
    }

    if (!started) {
      Update.printError(Serial);
      if (filesystem) LittleFS.begin();
      return;
    }
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.hasError()) return;
    const size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) Update.printError(Serial);
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaUploadSucceeded = true;
      Serial.print(filesystem ? "LittleFS upload voltooid: " : "Firmware upload voltooid: ");
      Serial.print(upload.totalSize);
      Serial.println(" bytes");
    } else {
      Update.printError(Serial);
      if (filesystem) LittleFS.begin();
    }
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.end();
    otaUploadSucceeded = false;
    if (filesystem) LittleFS.begin();
    Serial.println(filesystem ? "LittleFS upload afgebroken" : "Firmware upload afgebroken");
  }

  yield();
}

void handleFirmwareUploadFinished() { finishOtaResponse("Firmware"); }
void handleFirmwareUploadData() { processOtaUpload(false); }
void handleFilesystemUploadFinished() { finishOtaResponse("LittleFS"); }
void handleFilesystemUploadData() { processOtaUpload(true); }


void handleMqttSettingsGet() {
  const auto& cfg = settingsManager.mqtt();

  String json = "{";
  json += "\"enabled\":" + String(cfg.enabled ? "true" : "false");
  json += ",\"host\":\"" + escapeJson(cfg.host) + "\"";
  json += ",\"port\":" + String(cfg.port);
  json += ",\"username\":\"" + escapeJson(cfg.username) + "\"";
  json += ",\"clientId\":\"" + escapeJson(cfg.clientId) + "\"";
  json += ",\"baseTopic\":\"" + escapeJson(cfg.baseTopic) + "\"";
  json += ",\"homeAssistantDiscovery\":";
  json += cfg.homeAssistantDiscovery ? "true" : "false";
  json += "}";

  server.send(200, "application/json", json);
}

void handleMqttSettingsPost() {
  auto& cfg = settingsManager.mqtt();

  if (server.hasArg("enabled")) cfg.enabled = server.arg("enabled") == "true";
  if (server.hasArg("host")) cfg.host = server.arg("host");
  if (server.hasArg("port")) {
    const long port = server.arg("port").toInt();
    if (port < 1 || port > 65535) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"Ongeldige MQTT-poort\"}");
      return;
    }
    cfg.port = static_cast<uint16_t>(port);
  }
  if (server.hasArg("username")) cfg.username = server.arg("username");
  if (server.hasArg("password")) cfg.password = server.arg("password");
  if (server.hasArg("clientId")) cfg.clientId = server.arg("clientId");
  if (server.hasArg("baseTopic")) cfg.baseTopic = server.arg("baseTopic");
  if (server.hasArg("homeAssistantDiscovery")) {
    cfg.homeAssistantDiscovery = server.arg("homeAssistantDiscovery") == "true";
  }

  if (!settingsManager.save()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Opslaan mislukt\"}");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleRegionalSettingsGet() {
  const auto& cfg = settingsManager.regional();

  String json = "{";
  json += "\"language\":\"" + escapeJson(cfg.language) + "\"";
  json += ",\"timeZone\":\"" + escapeJson(cfg.timeZone) + "\"";
  json += ",\"use24HourClock\":";
  json += cfg.use24HourClock ? "true" : "false";
  json += ",\"dateFormat\":\"" + escapeJson(cfg.dateFormat) + "\"";
  json += ",\"temperatureUnit\":\"";
  json += SettingsManager::temperatureUnitToString(cfg.temperatureUnit);
  json += "\"}";

  server.send(200, "application/json", json);
}

void handleRegionalSettingsPost() {
  auto& cfg = settingsManager.regional();

  if (server.hasArg("language")) cfg.language = server.arg("language");
  if (server.hasArg("timeZone")) cfg.timeZone = server.arg("timeZone");
  if (server.hasArg("use24HourClock")) {
    cfg.use24HourClock = server.arg("use24HourClock") == "true";
  }
  if (server.hasArg("dateFormat")) cfg.dateFormat = server.arg("dateFormat");
  if (server.hasArg("temperatureUnit")) {
    cfg.temperatureUnit = SettingsManager::temperatureUnitFromString(
      server.arg("temperatureUnit"),
      cfg.temperatureUnit
    );
  }

  cfg.language.trim();
  cfg.timeZone.trim();
  cfg.dateFormat.trim();

  if (cfg.language.isEmpty() || cfg.timeZone.isEmpty()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Taal en tijdzone zijn verplicht\"}");
    return;
  }

  if (!settingsManager.save()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Opslaan mislukt\"}");
    return;
  }

  timeManager.forceResync();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleSystemSettingsGet() {
  String json = "{";
  json += "\"timeSynchronized\":";
  json += timeManager.isSynchronized() ? "true" : "false";
  json += ",\"date\":\"" + escapeJson(timeManager.formattedDate()) + "\"";
  json += ",\"time\":\"" + escapeJson(timeManager.formattedTime()) + "\"";
  json += ",\"timeZone\":\"" + escapeJson(timeManager.configuredTimeZone()) + "\"";
  json += ",\"schedulerCount\":" + String(schedulerManager.count());
  json += "}";

  server.send(200, "application/json", json);
}

// Backwards compatibility for older web files.
void handleSettingsGet() {
  const auto& mqtt = settingsManager.mqtt();
  const auto& region = settingsManager.regional();

  String json = "{";
  json += "\"enabled\":" + String(mqtt.enabled ? "true" : "false");
  json += ",\"host\":\"" + escapeJson(mqtt.host) + "\"";
  json += ",\"port\":" + String(mqtt.port);
  json += ",\"username\":\"" + escapeJson(mqtt.username) + "\"";
  json += ",\"clientId\":\"" + escapeJson(mqtt.clientId) + "\"";
  json += ",\"baseTopic\":\"" + escapeJson(mqtt.baseTopic) + "\"";
  json += ",\"homeAssistantDiscovery\":" + String(mqtt.homeAssistantDiscovery ? "true" : "false");
  json += ",\"language\":\"" + escapeJson(region.language) + "\"";
  json += ",\"timeZone\":\"" + escapeJson(region.timeZone) + "\"";
  json += ",\"use24HourClock\":" + String(region.use24HourClock ? "true" : "false");
  json += ",\"dateFormat\":\"" + escapeJson(region.dateFormat) + "\"";
  json += ",\"temperatureUnit\":\"";
  json += SettingsManager::temperatureUnitToString(region.temperatureUnit);
  json += "\"}";

  server.send(200, "application/json", json);
}

void handleSettingsPost() {
  auto& mqtt = settingsManager.mqtt();
  auto& region = settingsManager.regional();

  if (server.hasArg("enabled")) mqtt.enabled = server.arg("enabled") == "true";
  if (server.hasArg("host")) mqtt.host = server.arg("host");
  if (server.hasArg("port")) mqtt.port = server.arg("port").toInt();
  if (server.hasArg("username")) mqtt.username = server.arg("username");
  if (server.hasArg("password")) mqtt.password = server.arg("password");
  if (server.hasArg("clientId")) mqtt.clientId = server.arg("clientId");
  if (server.hasArg("baseTopic")) mqtt.baseTopic = server.arg("baseTopic");
  if (server.hasArg("homeAssistantDiscovery")) {
    mqtt.homeAssistantDiscovery = server.arg("homeAssistantDiscovery") == "true";
  }

  if (server.hasArg("language")) region.language = server.arg("language");
  if (server.hasArg("timeZone")) region.timeZone = server.arg("timeZone");
  if (server.hasArg("use24HourClock")) {
    region.use24HourClock = server.arg("use24HourClock") == "true";
  }
  if (server.hasArg("dateFormat")) region.dateFormat = server.arg("dateFormat");
  if (server.hasArg("temperatureUnit")) {
    region.temperatureUnit = SettingsManager::temperatureUnitFromString(
      server.arg("temperatureUnit"),
      region.temperatureUnit
    );
  }

  if (!settingsManager.save()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Opslaan mislukt\"}");
    return;
  }

  timeManager.forceResync();
  server.send(200, "application/json", "{\"ok\":true}");
}



void handleSchedulesGet() {
  String json = "[";
  for (uint8_t i = 0; i < schedulerManager.count(); i++) {
    const ScheduleItem* s = schedulerManager.get(i);
    if (!s) continue;
    if (json.length() > 1) json += ",";
    json += "{";
    json += "\"id\":" + String(s->id);
    json += ",\"enabled\":" + String(s->enabled ? "true":"false");
    json += ",\"daysMask\":" + String(s->daysMask);
    json += ",\"hour\":" + String(s->hour);
    json += ",\"minute\":" + String(s->minute);
    json += ",\"action\":" + String((uint8_t)s->action);
    json += ",\"value\":" + String(s->value);
    json += "}";
  }
  json += "]";
  server.send(200,"application/json",json);
}



void handleSchedulesPost() {
  ScheduleItem item;

  item.enabled = !server.hasArg("enabled") || server.arg("enabled") == "true";
  item.daysMask = server.arg("daysMask").toInt();
  item.hour = server.arg("hour").toInt();
  item.minute = server.arg("minute").toInt();
  item.action = static_cast<ScheduleAction>(server.arg("action").toInt());
  item.value = server.arg("value").toInt();

  if (item.hour > 23 || item.minute > 59 || item.daysMask == 0) {
    server.send(400,"application/json","{\"ok\":false,\"error\":\"Ongeldige invoer\"}");
    return;
  }

  if (!schedulerManager.add(item)) {
    server.send(500,"application/json","{\"ok\":false,\"error\":\"Schema niet opgeslagen\"}");
    return;
  }

  server.send(200,"application/json","{\"ok\":true}");
}



void handleSchedulesPut() {
  if (!server.hasArg("id")) {
    server.send(400,"application/json","{\"ok\":false,\"error\":\"id ontbreekt\"}");
    return;
  }

  ScheduleItem item;
  const uint16_t id = server.arg("id").toInt();
  item.enabled = !server.hasArg("enabled") || server.arg("enabled")=="true";
  item.daysMask = server.arg("daysMask").toInt();
  item.hour = server.arg("hour").toInt();
  item.minute = server.arg("minute").toInt();
  item.action = static_cast<ScheduleAction>(server.arg("action").toInt());
  item.value = server.arg("value").toInt();

  if (!schedulerManager.update(id,item)) {
    server.send(404,"application/json","{\"ok\":false,\"error\":\"Schema niet gevonden\"}");
    return;
  }

  server.send(200,"application/json","{\"ok\":true}");
}

void handleSchedulesDelete() {
  if (!server.hasArg("id")) {
    server.send(400,"application/json","{\"ok\":false,\"error\":\"id ontbreekt\"}");
    return;
  }

  if (!schedulerManager.remove(server.arg("id").toInt())) {
    server.send(404,"application/json","{\"ok\":false,\"error\":\"Schema niet gevonden\"}");
    return;
  }

  server.send(200,"application/json","{\"ok\":true}");
}



void handleHistoryGet() {
  uint16_t limit = server.hasArg("limit") ? server.arg("limit").toInt() : 288;
  const uint16_t total = historyManager.count();
  if (limit == 0 || limit > total) limit = total;
  const uint16_t start = total - limit;
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  for (uint16_t i = start; i < total; i++) {
    const HistorySample* sample = historyManager.get(i);
    if (!sample) continue;
    String item;
    item.reserve(128);
    if (i > start) item += ',';
    item += "{\"timestamp\":" + String((unsigned long)sample->timestamp);
    item += ",\"temperature\":" + String(sample->temperature);
    item += ",\"target\":" + String(sample->targetTemperature);
    item += ",\"heater\":" + String((sample->flags & 1) ? "true" : "false");
    item += ",\"filter\":" + String((sample->flags & 2) ? "true" : "false");
    item += ",\"bubbles\":" + String((sample->flags & 4) ? "true" : "false");
    item += ",\"jets\":" + String((sample->flags & 8) ? "true" : "false");
    item += "}";
    server.sendContent(item);
    yield();
  }
  server.sendContent("]");
  server.sendContent("");
}
void handleHistoryDelete() {
  const bool ok = historyManager.clear();
  eventLog.info("Historie gewist");
  server.send(ok ? 200 : 500, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}
void handleEnergyGet() { server.send(200, "application/json", energyManager.toJson()); }
void handleEnergyPost() {
  auto& cfg = settingsManager.energy();
  if (server.hasArg("heaterWatts")) cfg.heaterWatts = server.arg("heaterWatts").toInt();
  if (server.hasArg("filterWatts")) cfg.filterWatts = server.arg("filterWatts").toInt();
  if (server.hasArg("bubblesWatts")) cfg.bubblesWatts = server.arg("bubblesWatts").toInt();
  if (server.hasArg("jetsWatts")) cfg.jetsWatts = server.arg("jetsWatts").toInt();
  if (server.hasArg("pricePerKwh")) cfg.pricePerKwh = server.arg("pricePerKwh").toFloat();
  if (server.hasArg("currency")) cfg.currency = server.arg("currency");
  if (!settingsManager.save()) { server.send(500,"application/json","{\"ok\":false}"); return; }
  eventLog.info("Energie-instellingen gewijzigd");
  server.send(200,"application/json","{\"ok\":true}");
}
void handleEnergyDelete() {
  const bool ok=energyManager.reset();
  eventLog.info("Energietellers gereset");
  server.send(ok?200:500,"application/json",ok?"{\"ok\":true}":"{\"ok\":false}");
}
void handleLogsGet() { uint16_t limit=server.hasArg("limit")?server.arg("limit").toInt():50; server.send(200,"application/json",eventLog.toJson(limit)); }
void handleLogsDelete() { eventLog.clear(); server.send(200,"application/json","{\"ok\":true}"); }


void handleDiagnosticsGet() {
  FSInfo fsInfo;
  const bool fsOk = LittleFS.info(fsInfo);
  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t maxBlock = ESP.getMaxFreeBlockSize();
  const uint8_t fragmentation = ESP.getHeapFragmentation();

  String json = "{";
  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"mqttConnected\":" + String(mqttManager.isConnected() ? "true" : "false");
  json += ",\"timeSynchronized\":" + String(timeManager.isSynchronized() ? "true" : "false");
  json += ",\"spaConnected\":" + String(spa.connected ? "true" : "false");
  json += ",\"spaDataValid\":" + String(spa.dataValid ? "true" : "false");
  json += ",\"freeHeap\":" + String(freeHeap);
  json += ",\"maxFreeBlock\":" + String(maxBlock);
  json += ",\"heapFragmentation\":" + String(fragmentation);
  json += ",\"flashSize\":" + String(ESP.getFlashChipRealSize());
  json += ",\"sketchSize\":" + String(ESP.getSketchSize());
  json += ",\"freeSketchSpace\":" + String(ESP.getFreeSketchSpace());
  json += ",\"cpuMhz\":" + String(ESP.getCpuFreqMHz());
  json += ",\"resetReason\":\"" + escapeJson(ESP.getResetReason()) + "\"";
  json += ",\"rssi\":" + String(WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0);
  json += ",\"ip\":\"" + escapeJson(WiFi.localIP().toString()) + "\"";
  json += ",\"uptimeMs\":" + String(millis());
  json += ",\"filesystemOk\":" + String(fsOk ? "true" : "false");
  json += ",\"filesystemUsed\":" + String(fsOk ? fsInfo.usedBytes : 0);
  json += ",\"filesystemTotal\":" + String(fsOk ? fsInfo.totalBytes : 0);
  json += ",\"historyCount\":" + String(historyManager.count());
  json += ",\"scheduleCount\":" + String(schedulerManager.count());
  json += "}";

  server.send(200, "application/json", json);
}

void handleSelfTest() {
  FSInfo fsInfo;
  const bool fsOk = LittleFS.info(fsInfo);
  const bool heapOk = ESP.getFreeHeap() >= 12000;
  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  const bool timeOk = timeManager.isSynchronized();
  const bool spaOk = spa.connected && spa.dataValid;
  const bool mqttRequired = settingsManager.mqtt().enabled;
  const bool mqttOk = !mqttRequired || mqttManager.isConnected();
  const bool overall = fsOk && heapOk && wifiOk && timeOk && mqttOk;

  String json = "{";
  json += "\"ok\":" + String(overall ? "true" : "false");
  json += ",\"filesystem\":" + String(fsOk ? "true" : "false");
  json += ",\"memory\":" + String(heapOk ? "true" : "false");
  json += ",\"wifi\":" + String(wifiOk ? "true" : "false");
  json += ",\"time\":" + String(timeOk ? "true" : "false");
  json += ",\"mqtt\":" + String(mqttOk ? "true" : "false");
  json += ",\"spa\":" + String(spaOk ? "true" : "false");
  json += ",\"note\":\"Zelftest schakelt geen spa-functies\"";
  json += "}";

  eventLog.info(overall ? "Zelftest geslaagd" : "Zelftest aandachtspunten gevonden");
  server.send(overall ? 200 : 207, "application/json", json);
}

void webAppBegin() {
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/api/wifi/forget", HTTP_POST, handleWifiForget);

  server.on(
    "/api/ota",
    HTTP_POST,
    handleFirmwareUploadFinished,
    handleFirmwareUploadData
  );

  server.on(
    "/api/ota/filesystem",
    HTTP_POST,
    handleFilesystemUploadFinished,
    handleFilesystemUploadData
  );

  server.serveStatic("/manifest.json", LittleFS, "/manifest.json");
  server.serveStatic("/sw.js", LittleFS, "/sw.js");
  server.serveStatic("/icons", LittleFS, "/icons");

  server.on("/", handleRoot);

  server.serveStatic("/css", LittleFS, "/css");
  server.serveStatic("/js", LittleFS, "/js");
  server.serveStatic("/pages", LittleFS, "/pages");
  server.serveStatic("/lang", LittleFS, "/lang");

  server.on("/api/status", handleStatus);
  server.on("/api/restart", handleRestart);
  server.on("/api/settings", HTTP_GET, handleSettingsGet);
  server.on("/api/settings", HTTP_POST, handleSettingsPost);
  server.on("/api/settings/mqtt", HTTP_GET, handleMqttSettingsGet);
  server.on("/api/settings/mqtt", HTTP_POST, handleMqttSettingsPost);
  server.on("/api/settings/region", HTTP_GET, handleRegionalSettingsGet);
  server.on("/api/settings/region", HTTP_POST, handleRegionalSettingsPost);
  server.on("/api/settings/system", HTTP_GET, handleSystemSettingsGet);
  server.on("/api/hardware", HTTP_GET, handleHardwareGet);
  server.on("/api/hardware", HTTP_POST, handleHardwarePost);
  server.on("/api/schedules", HTTP_GET, handleSchedulesGet);
  server.on("/api/schedules", HTTP_POST, handleSchedulesPost);
  server.on("/api/schedules", HTTP_PUT, handleSchedulesPut);
  server.on("/api/schedules", HTTP_DELETE, handleSchedulesDelete);
  server.on("/api/history", HTTP_GET, handleHistoryGet);
  server.on("/api/history", HTTP_DELETE, handleHistoryDelete);
  server.on("/api/energy", HTTP_GET, handleEnergyGet);
  server.on("/api/energy", HTTP_POST, handleEnergyPost);
  server.on("/api/energy", HTTP_DELETE, handleEnergyDelete);
  server.on("/api/logs", HTTP_GET, handleLogsGet);
  server.on("/api/logs", HTTP_DELETE, handleLogsDelete);
  server.on("/api/diagnostics", HTTP_GET, handleDiagnosticsGet);
  server.on("/api/selftest", HTTP_POST, handleSelfTest);


  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Webserver gestart");
  Serial.println("WebSocket gestart op poort 81");
}

void webAppLoop() {
  server.handleClient();
  webSocket.loop();

  if (otaRestartPending &&
      static_cast<long>(millis() - otaRestartAt) >= 0) {
    ESP.restart();
  }

  if (millis() - lastBroadcast > 1000) {
    lastBroadcast = millis();
    webAppBroadcast();
  }
}