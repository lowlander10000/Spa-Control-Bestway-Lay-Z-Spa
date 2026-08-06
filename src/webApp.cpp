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
#include "maintenanceManager.h"

#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <Updater.h>
#include <ArduinoJson.h>

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

unsigned long lastBroadcast = 0;
uint32_t minimumObservedHeap = UINT32_MAX;

bool otaUploadSucceeded = false;
bool otaFilesystemUpload = false;
bool otaRestartPending = false;
unsigned long otaRestartAt = 0;
bool filesystemUpdateInProgress = false;

String escapeJson(String value);

namespace {

constexpr size_t BACKUP_JSON_CAPACITY = 4096;
constexpr size_t GENERIC_JSON_CAPACITY = 4096;

char lastOtaError[192] = "";

void setDebugMessage(char* target, size_t targetSize, const String& message, bool logError = true) {
  const String clean = message.substring(0, targetSize - 1);
  strncpy(target, clean.c_str(), targetSize - 1);
  target[targetSize - 1] = '\0';
  Serial.println(clean);
  if (logError) eventLog.error(clean);
}

void clearDebugMessage(char* target, size_t targetSize) {
  if (targetSize > 0) target[0] = '\0';
}

bool validateJsonFile(const String& path, DynamicJsonDocument& document) {
  document.clear();
  File file = LittleFS.open(path, "r");
  if (!file) return false;
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  return !error && !document.overflowed();
}

bool copyFileSafely(const String& sourcePath, const String& destinationPath) {
  File source = LittleFS.open(sourcePath, "r");
  if (!source) return false;

  const String tempPath = destinationPath + ".tmp";
  LittleFS.remove(tempPath);
  File destination = LittleFS.open(tempPath, "w");
  if (!destination) {
    source.close();
    return false;
  }

  uint8_t buffer[256];
  size_t totalWritten = 0;
  while (source.available()) {
    const size_t bytesRead = source.read(buffer, sizeof(buffer));
    if (bytesRead == 0) break;
    const size_t bytesWritten = destination.write(buffer, bytesRead);
    if (bytesWritten != bytesRead) {
      source.close();
      destination.close();
      LittleFS.remove(tempPath);
      return false;
    }
    totalWritten += bytesWritten;
    yield();
  }
  source.close();
  destination.flush();
  destination.close();

  if (totalWritten == 0) {
    LittleFS.remove(tempPath);
    return false;
  }

  LittleFS.remove(destinationPath);
  if (!LittleFS.rename(tempPath, destinationPath)) {
    LittleFS.remove(tempPath);
    return false;
  }
  return true;
}

bool writeJsonFileAtomically(const char* path, const JsonDocument& document) {
  if (document.overflowed()) {
    Serial.printf("JSON-opslag geweigerd: document overflow voor %s\n", path);
    return false;
  }

  const size_t measuredSize = measureJson(document);
  if (measuredSize == 0 || measuredSize > GENERIC_JSON_CAPACITY) {
    Serial.printf("JSON-opslag geweigerd: ongeldige grootte %u voor %s\n",
                  static_cast<unsigned>(measuredSize), path);
    return false;
  }

  const String primaryPath(path);
  const String newPath = primaryPath + ".new";
  const String backupPath = primaryPath + ".bak";
  LittleFS.remove(newPath);

  File pending = LittleFS.open(newPath, "w");
  if (!pending) {
    Serial.printf("JSON-opslag: %s kon niet worden geopend\n", newPath.c_str());
    return false;
  }

  const size_t written = serializeJson(document, pending);
  pending.flush();
  pending.close();
  if (written != measuredSize) {
    Serial.printf("JSON-opslag: slechts %u van %u bytes geschreven naar %s\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(measuredSize),
                  newPath.c_str());
    LittleFS.remove(newPath);
    return false;
  }

  // Valideer het tijdelijke bestand. De buffer bestaat alleen in dit blok.
  {
    DynamicJsonDocument check(GENERIC_JSON_CAPACITY);
    if (!validateJsonFile(newPath, check)) {
      Serial.printf("JSON-opslag: nieuw bestand %s is ongeldig\n", newPath.c_str());
      LittleFS.remove(newPath);
      return false;
    }
  }

  // Bewaar uitsluitend een aantoonbaar geldig primair bestand.
  if (LittleFS.exists(primaryPath)) {
    bool currentValid = false;
    {
      DynamicJsonDocument check(GENERIC_JSON_CAPACITY);
      currentValid = validateJsonFile(primaryPath, check);
    }
    if (currentValid && !copyFileSafely(primaryPath, backupPath)) {
      Serial.printf("JSON-opslag: waarschuwing, back-up %s kon niet worden bijgewerkt\n",
                    backupPath.c_str());
    }
  }

  LittleFS.remove(primaryPath);
  if (!LittleFS.rename(newPath, primaryPath)) {
    Serial.printf("JSON-opslag: hernoemen van %s naar %s mislukt\n",
                  newPath.c_str(), primaryPath.c_str());
    LittleFS.remove(newPath);
    if (LittleFS.exists(backupPath)) copyFileSafely(backupPath, primaryPath);
    return false;
  }

  bool finalValid = false;
  {
    DynamicJsonDocument check(GENERIC_JSON_CAPACITY);
    finalValid = validateJsonFile(primaryPath, check);
  }
  if (!finalValid) {
    Serial.printf("JSON-opslag: eindcontrole van %s mislukt; rollback wordt geprobeerd\n",
                  primaryPath.c_str());
    LittleFS.remove(primaryPath);
    if (LittleFS.exists(backupPath)) copyFileSafely(backupPath, primaryPath);
    return false;
  }
  return true;
}


bool validateJsonSyntaxLowMemory(const String& content) {
  StaticJsonDocument<32> filter;
  filter["__spa_probe__"] = true;
  StaticJsonDocument<64> probe;
  const DeserializationError error = deserializeJson(
    probe,
    content,
    DeserializationOption::Filter(filter)
  );
  return !error && probe.is<JsonObject>();
}

bool writeRawJsonFileAtomically(const char* path, const String& content) {
  if (content.length() < 2 || content.length() > 16384 || !validateJsonSyntaxLowMemory(content)) {
    Serial.printf("Back-up herstel geweigerd: ongeldige JSON voor %s (%u bytes)\n",
                  path, static_cast<unsigned>(content.length()));
    return false;
  }

  const String primaryPath(path);
  const String newPath = primaryPath + ".new";
  const String backupPath = primaryPath + ".bak";
  LittleFS.remove(newPath);

  File pending = LittleFS.open(newPath, "w");
  if (!pending) {
    Serial.printf("Back-up herstel: %s kon niet worden geopend\n", newPath.c_str());
    return false;
  }

  const size_t written = pending.print(content);
  pending.flush();
  pending.close();
  if (written != content.length()) {
    Serial.printf("Back-up herstel: slechts %u van %u bytes geschreven naar %s\n",
                  static_cast<unsigned>(written),
                  static_cast<unsigned>(content.length()),
                  newPath.c_str());
    LittleFS.remove(newPath);
    return false;
  }

  File verify = LittleFS.open(newPath, "r");
  const size_t verifiedSize = verify ? verify.size() : 0;
  if (verify) verify.close();
  if (verifiedSize != content.length()) {
    Serial.printf("Back-up herstel: groottecontrole mislukt voor %s\n", newPath.c_str());
    LittleFS.remove(newPath);
    return false;
  }

  if (LittleFS.exists(primaryPath) && !copyFileSafely(primaryPath, backupPath)) {
    Serial.printf("Back-up herstel: waarschuwing, bestaand bestand %s kon niet worden geback-upt\n",
                  primaryPath.c_str());
  }

  LittleFS.remove(primaryPath);
  if (!LittleFS.rename(newPath, primaryPath)) {
    Serial.printf("Back-up herstel: hernoemen van %s naar %s mislukt\n",
                  newPath.c_str(), primaryPath.c_str());
    LittleFS.remove(newPath);
    if (LittleFS.exists(backupPath)) copyFileSafely(backupPath, primaryPath);
    return false;
  }
  return true;
}

const char* backupFilePath(const String& name) {
  if (name == "settings") return "/settings.json";
  if (name == "hardware") return "/bestway_hwcfg.json";
  if (name == "maintenance") return "/maintenance.json";
  return nullptr;
}

void handleBackupFileGet() {
  const char* path = backupFilePath(server.arg("name"));
  if (!path) { server.send(400, "application/json", "{\"ok\":false,\"error\":\"Onbekend back-uponderdeel\"}"); return; }
  File file = LittleFS.open(path, "r");
  if (!file) { server.send(404, "application/json", "{\"ok\":false,\"error\":\"Bestand ontbreekt\"}"); return; }
  server.streamFile(file, "application/json");
  file.close();
}

void handleBackupFilePost() {
  const String name = server.arg("name");
  const char* path = backupFilePath(name);
  if (!path || !server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Ongeldige herstelopdracht\"}");
    return;
  }

  String content = server.arg("plain");

  // Een lege MQTT-wachtwoordwaarde uit een geëxporteerde back-up mag het
  // huidige wachtwoord niet wissen. WiFi-instellingen zitten niet in deze
  // back-up en worden door deze route nooit gewijzigd.
  if (name == "settings" && settingsManager.mqtt().password.length() > 0) {
    const String escapedPassword = escapeJson(settingsManager.mqtt().password);
    content.replace("\"password\":\"\"", "\"password\":\"" + escapedPassword + "\"");
    content.replace("\"password\": \"\"", "\"password\": \"" + escapedPassword + "\"");
  }

  if (!validateJsonSyntaxLowMemory(content)) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"Ongeldige JSON\"}");
    return;
  }
  if (!writeRawJsonFileAtomically(path, content)) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Herstellen mislukt\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

}

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


const char* updateErrorText(uint8_t error) {
  switch (error) {
    case UPDATE_ERROR_OK: return "Geen fout";
    case UPDATE_ERROR_WRITE: return "Flash schrijven mislukt";
    case UPDATE_ERROR_ERASE: return "Flash wissen mislukt";
    case UPDATE_ERROR_READ: return "Flash lezen mislukt";
    case UPDATE_ERROR_SPACE: return "Onvoldoende ruimte in updatepartitie";
    case UPDATE_ERROR_SIZE: return "Ongeldige updategrootte";
    case UPDATE_ERROR_STREAM: return "Uploadstream onderbroken";
    case UPDATE_ERROR_MD5: return "MD5-controle mislukt";
    case UPDATE_ERROR_FLASH_CONFIG: return "Flashconfiguratie komt niet overeen";
    case UPDATE_ERROR_NEW_FLASH_CONFIG: return "Nieuwe flashconfiguratie is ongeldig";
    case UPDATE_ERROR_MAGIC_BYTE: return "Ongeldig firmware- of filesystembestand";
    case UPDATE_ERROR_BOOTSTRAP: return "Bootstrap-update mislukt";
    case UPDATE_ERROR_SIGN: return "Ondertekeningscontrole mislukt";
    default: return "Onbekende updatefout";
  }
}

String updateErrorJson(const char* typeName) {
  const uint8_t code = Update.getError();
  String json = "{\"ok\":false,\"error\":\"";
  json += typeName;
  json += "-update mislukt: ";
  json += updateErrorText(code);
  json += " (code ";
  json += String(code);
  json += ")\",\"code\":";
  json += String(code);
  json += "}";
  return json;
}

void finishOtaResponse(const char* typeName) {
  if (Update.hasError() || !otaUploadSucceeded) {
    const String error = updateErrorJson(typeName);
    Serial.print(typeName);
    Serial.print(" eindstatus fout: ");
    Serial.println(error);

    if (otaFilesystemUpload) {
      const bool mounted = LittleFS.begin();
      Serial.println(mounted ? "LittleFS opnieuw gekoppeld na fout"
                             : "LittleFS opnieuw koppelen na fout MISLUKT");
      filesystemUpdateInProgress = false;
    }

    setDebugMessage(lastOtaError, sizeof(lastOtaError), String(typeName) + ": " + error);
    server.send(500, "application/json", error);
    return;
  }

  clearDebugMessage(lastOtaError, sizeof(lastOtaError));
  eventLog.info(String(typeName) + "-update voltooid");
  String message = String("{\"ok\":true,\"message\":\"") + typeName +
                   "-update voltooid. ESP start opnieuw op.\"}";
  server.send(200, "application/json", message);
  otaRestartPending = true;
  otaRestartAt = millis() + 1200;
}

void processOtaUpload(bool filesystem) {
  static size_t receivedBytes = 0;
  static size_t lastReportedBytes = 0;
  static uint32_t uploadStartedAt = 0;
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaUploadSucceeded = false;
    otaFilesystemUpload = filesystem;
    receivedBytes = 0;
    lastReportedBytes = 0;
    uploadStartedAt = millis();

    Serial.println();
    Serial.println("========== OTA UPLOAD START ==========");
    Serial.print("Type: ");
    Serial.println(filesystem ? "LittleFS" : "Firmware");
    Serial.print("Bestand: ");
    Serial.println(upload.filename);
    Serial.print("Vrij heapgeheugen: ");
    Serial.println(ESP.getFreeHeap());

    bool started = false;
    if (filesystem) {
      FSInfo fsInfo;
      if (LittleFS.info(fsInfo)) {
        Serial.print("LittleFS logisch totaal: ");
        Serial.println(fsInfo.totalBytes);
        Serial.print("LittleFS logisch gebruikt: ");
        Serial.println(fsInfo.usedBytes);
      } else {
        Serial.println("Waarschuwing: LittleFS.info() mislukt");
      }

      // Diagnostische RC5: geen EEPROM-configuratieback-up. Hiermee sluiten
      // we de backupstap uit als oorzaak van de vastlopende upload.
      filesystemUpdateInProgress = true;
      LittleFS.end();
      delay(50);

      // HTTPUpload.totalSize is bij UPLOAD_FILE_START nog 0 en groeit pas
      // terwijl de upload binnenkomt. Gebruik daarom de beschikbare
      // LittleFS-partitiegrootte als bovengrens voor de U_FS-update.
      FSInfo updateFsInfo;
      size_t filesystemCapacity = 0;
      if (LittleFS.begin() && LittleFS.info(updateFsInfo)) {
        filesystemCapacity = updateFsInfo.totalBytes;
        LittleFS.end();
      }

      Serial.print("HTTP multipart contentLength: ");
      Serial.println(upload.contentLength);
      Serial.print("LittleFS updatecapaciteit: ");
      Serial.println(filesystemCapacity);

      if (filesystemCapacity > 0) {
        started = Update.begin(filesystemCapacity, U_FS);
      } else {
        Serial.println("Update.begin niet gestart: LittleFS-capaciteit onbekend");
      }
      Serial.println("Configuratieback-up voor deze test: UITGESCHAKELD");
    } else {
      const uint32_t maxSketchSpace =
        (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
      Serial.print("Maximale firmwareruimte: ");
      Serial.println(maxSketchSpace);
      started = Update.begin(maxSketchSpace, U_FLASH);
    }

    Serial.print("Update.begin: ");
    Serial.println(started ? "OK" : "MISLUKT");

    if (!started) {
      Update.printError(Serial);
      Serial.print("Update error code: ");
      Serial.println(Update.getError());
      Serial.print("Update error tekst: ");
      Serial.println(updateErrorText(Update.getError()));
      if (filesystem) {
        const bool mounted = LittleFS.begin();
        Serial.println(mounted ? "LittleFS opnieuw gekoppeld"
                               : "LittleFS opnieuw koppelen MISLUKT");
        filesystemUpdateInProgress = false;
      }
      return;
    }
  }

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.hasError()) {
      Serial.print("WRITE overgeslagen wegens eerdere fout, code ");
      Serial.println(Update.getError());
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    receivedBytes += written;

    if (written != upload.currentSize) {
      Serial.print("Korte write: aangeboden=");
      Serial.print(upload.currentSize);
      Serial.print(", geschreven=");
      Serial.println(written);
      Update.printError(Serial);
      Serial.print("Update error code: ");
      Serial.println(Update.getError());
      Serial.print("Update error tekst: ");
      Serial.println(updateErrorText(Update.getError()));
    }

    if (receivedBytes - lastReportedBytes >= 65536 || written != upload.currentSize) {
      lastReportedBytes = receivedBytes;
      Serial.print("Ontvangen/geschreven: ");
      Serial.print(receivedBytes);
      Serial.print(" bytes, chunk: ");
      Serial.print(upload.currentSize);
      Serial.print(", heap: ");
      Serial.println(ESP.getFreeHeap());
    }
  }

  if (upload.status == UPLOAD_FILE_END) {
    Serial.print("UPLOAD_FILE_END totalSize: ");
    Serial.println(upload.totalSize);
    Serial.print("Zelf getelde bytes: ");
    Serial.println(receivedBytes);
    Serial.print("Duur ms: ");
    Serial.println(millis() - uploadStartedAt);

    if (Update.end(true)) {
      otaUploadSucceeded = true;
      Serial.println(filesystem ? "LittleFS Update.end(true): OK"
                                : "Firmware Update.end(true): OK");
    } else {
      Serial.println("Update.end(true): MISLUKT");
      Update.printError(Serial);
      Serial.print("Update error code: ");
      Serial.println(Update.getError());
      Serial.print("Update error tekst: ");
      Serial.println(updateErrorText(Update.getError()));
      if (filesystem) {
        const bool mounted = LittleFS.begin();
        Serial.println(mounted ? "LittleFS opnieuw gekoppeld na end-fout"
                               : "LittleFS opnieuw koppelen na end-fout MISLUKT");
        filesystemUpdateInProgress = false;
      }
    }
    Serial.println("=========== OTA UPLOAD END ===========");
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    Serial.println("UPLOAD_FILE_ABORTED ontvangen");
    Serial.print("Bytes voor afbreken: ");
    Serial.println(receivedBytes);
    Serial.print("Update error code: ");
    Serial.println(Update.getError());
    Update.end();
    otaUploadSucceeded = false;
    if (filesystem) {
      const bool mounted = LittleFS.begin();
      Serial.println(mounted ? "LittleFS opnieuw gekoppeld na afbreken"
                             : "LittleFS opnieuw koppelen na afbreken MISLUKT");
      filesystemUpdateInProgress = false;
    }
  }

  yield();
}


void handleFirmwareUploadFinished() { finishOtaResponse("Firmware"); }
void handleFirmwareUploadData() { processOtaUpload(false); }
void handleFilesystemUploadFinished() { finishOtaResponse("LittleFS"); }
void handleFilesystemUploadData() { processOtaUpload(true); }


void handleMqttSettingsGet() {
  const auto& cfg = settingsManager.mqtt();

  String json;
  json.reserve(1800);
  json = "{";
  json += "\"enabled\":" + String(cfg.enabled ? "true" : "false");
  json += ",\"host\":\"" + escapeJson(cfg.host) + "\"";
  json += ",\"port\":" + String(cfg.port);
  json += ",\"username\":\"" + escapeJson(cfg.username) + "\"";
  json += ",\"clientId\":\"" + escapeJson(cfg.clientId) + "\"";
  json += ",\"baseTopic\":\"" + escapeJson(cfg.baseTopic) + "\"";
  json += ",\"homeAssistantDiscovery\":";
  json += cfg.homeAssistantDiscovery ? "true" : "false";
  json += ",\"publishTemperature\":" + String(cfg.publishTemperature ? "true" : "false");
  json += ",\"publishTarget\":" + String(cfg.publishTarget ? "true" : "false");
  json += ",\"publishPower\":" + String(cfg.publishPower ? "true" : "false");
  json += ",\"publishHeater\":" + String(cfg.publishHeater ? "true" : "false");
  json += ",\"publishHeatingActive\":" + String(cfg.publishHeatingActive ? "true" : "false");
  json += ",\"publishFilter\":" + String(cfg.publishFilter ? "true" : "false");
  json += ",\"publishBubbles\":" + String(cfg.publishBubbles ? "true" : "false");
  json += ",\"publishJets\":" + String(cfg.publishJets ? "true" : "false");
  json += ",\"publishLocked\":" + String(cfg.publishLocked ? "true" : "false");
  json += ",\"publishConnected\":" + String(cfg.publishConnected ? "true" : "false");
  json += ",\"publishReady\":" + String(cfg.publishReady ? "true" : "false");
  json += ",\"publishRssi\":" + String(cfg.publishRssi ? "true" : "false");
  json += ",\"publishHeap\":" + String(cfg.publishHeap ? "true" : "false");
  json += ",\"publishUptime\":" + String(cfg.publishUptime ? "true" : "false");
  json += ",\"publishFirmware\":" + String(cfg.publishFirmware ? "true" : "false");
  json += ",\"publishIp\":" + String(cfg.publishIp ? "true" : "false");
  json += ",\"publishJson\":" + String(cfg.publishJson ? "true" : "false");
  json += ",\"publishMaintenance\":" + String(cfg.publishMaintenance ? "true" : "false");
  json += ",\"topicTemperature\":\"" + escapeJson(cfg.topicTemperature) + "\"";
  json += ",\"topicTarget\":\"" + escapeJson(cfg.topicTarget) + "\"";
  json += ",\"topicPower\":\"" + escapeJson(cfg.topicPower) + "\"";
  json += ",\"topicHeater\":\"" + escapeJson(cfg.topicHeater) + "\"";
  json += ",\"topicHeatingActive\":\"" + escapeJson(cfg.topicHeatingActive) + "\"";
  json += ",\"topicFilter\":\"" + escapeJson(cfg.topicFilter) + "\"";
  json += ",\"topicBubbles\":\"" + escapeJson(cfg.topicBubbles) + "\"";
  json += ",\"topicJets\":\"" + escapeJson(cfg.topicJets) + "\"";
  json += ",\"topicLocked\":\"" + escapeJson(cfg.topicLocked) + "\"";
  json += ",\"topicConnected\":\"" + escapeJson(cfg.topicConnected) + "\"";
  json += ",\"topicReady\":\"" + escapeJson(cfg.topicReady) + "\"";
  json += ",\"topicRssi\":\"" + escapeJson(cfg.topicRssi) + "\"";
  json += ",\"topicHeap\":\"" + escapeJson(cfg.topicHeap) + "\"";
  json += ",\"topicUptime\":\"" + escapeJson(cfg.topicUptime) + "\"";
  json += ",\"topicFirmware\":\"" + escapeJson(cfg.topicFirmware) + "\"";
  json += ",\"topicIp\":\"" + escapeJson(cfg.topicIp) + "\"";
  json += ",\"topicJson\":\"" + escapeJson(cfg.topicJson) + "\"";
  json += ",\"topicMaintenance\":\"" + escapeJson(cfg.topicMaintenance) + "\"";
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
  if (server.hasArg("publishTemperature")) cfg.publishTemperature = server.arg("publishTemperature") == "true";
  if (server.hasArg("publishTarget")) cfg.publishTarget = server.arg("publishTarget") == "true";
  if (server.hasArg("publishPower")) cfg.publishPower = server.arg("publishPower") == "true";
  if (server.hasArg("publishHeater")) cfg.publishHeater = server.arg("publishHeater") == "true";
  if (server.hasArg("publishHeatingActive")) cfg.publishHeatingActive = server.arg("publishHeatingActive") == "true";
  if (server.hasArg("publishFilter")) cfg.publishFilter = server.arg("publishFilter") == "true";
  if (server.hasArg("publishBubbles")) cfg.publishBubbles = server.arg("publishBubbles") == "true";
  if (server.hasArg("publishJets")) cfg.publishJets = server.arg("publishJets") == "true";
  if (server.hasArg("publishLocked")) cfg.publishLocked = server.arg("publishLocked") == "true";
  if (server.hasArg("publishConnected")) cfg.publishConnected = server.arg("publishConnected") == "true";
  if (server.hasArg("publishReady")) cfg.publishReady = server.arg("publishReady") == "true";
  if (server.hasArg("publishRssi")) cfg.publishRssi = server.arg("publishRssi") == "true";
  if (server.hasArg("publishHeap")) cfg.publishHeap = server.arg("publishHeap") == "true";
  if (server.hasArg("publishUptime")) cfg.publishUptime = server.arg("publishUptime") == "true";
  if (server.hasArg("publishFirmware")) cfg.publishFirmware = server.arg("publishFirmware") == "true";
  if (server.hasArg("publishIp")) cfg.publishIp = server.arg("publishIp") == "true";
  if (server.hasArg("publishJson")) cfg.publishJson = server.arg("publishJson") == "true";
  if (server.hasArg("publishMaintenance")) cfg.publishMaintenance = server.arg("publishMaintenance") == "true";
  if (server.hasArg("topicTemperature")) cfg.topicTemperature = server.arg("topicTemperature");
  if (server.hasArg("topicTarget")) cfg.topicTarget = server.arg("topicTarget");
  if (server.hasArg("topicPower")) cfg.topicPower = server.arg("topicPower");
  if (server.hasArg("topicHeater")) cfg.topicHeater = server.arg("topicHeater");
  if (server.hasArg("topicHeatingActive")) cfg.topicHeatingActive = server.arg("topicHeatingActive");
  if (server.hasArg("topicFilter")) cfg.topicFilter = server.arg("topicFilter");
  if (server.hasArg("topicBubbles")) cfg.topicBubbles = server.arg("topicBubbles");
  if (server.hasArg("topicJets")) cfg.topicJets = server.arg("topicJets");
  if (server.hasArg("topicLocked")) cfg.topicLocked = server.arg("topicLocked");
  if (server.hasArg("topicConnected")) cfg.topicConnected = server.arg("topicConnected");
  if (server.hasArg("topicReady")) cfg.topicReady = server.arg("topicReady");
  if (server.hasArg("topicRssi")) cfg.topicRssi = server.arg("topicRssi");
  if (server.hasArg("topicHeap")) cfg.topicHeap = server.arg("topicHeap");
  if (server.hasArg("topicUptime")) cfg.topicUptime = server.arg("topicUptime");
  if (server.hasArg("topicFirmware")) cfg.topicFirmware = server.arg("topicFirmware");
  if (server.hasArg("topicIp")) cfg.topicIp = server.arg("topicIp");
  if (server.hasArg("topicJson")) cfg.topicJson = server.arg("topicJson");
  if (server.hasArg("topicMaintenance")) cfg.topicMaintenance = server.arg("topicMaintenance");

  if (!settingsManager.save()) {
    server.send(500, "application/json", "{\"ok\":false,\"error\":\"Opslaan mislukt\"}");
    return;
  }

  mqttManager.publishState();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMaintenanceGet() { server.send(200,"application/json",maintenanceManager.toJson()); }
void handleMaintenancePost() {
  auto b=[&](const char* key,bool fallback){return server.hasArg(key)?server.arg(key)=="true":fallback;};
  auto d=[&](const char* key,uint16_t fallback){long v=server.hasArg(key)?server.arg(key).toInt():fallback;return (uint16_t)constrain(v,1,3650);};
  const auto& fr=maintenanceManager.filterReplace();const auto& fc=maintenanceManager.filterClean();const auto& ch=maintenanceManager.chlorine();
  bool ok=maintenanceManager.updateSettings(b("filterReplaceEnabled",fr.enabled),d("filterReplaceDays",fr.intervalDays),b("filterCleanEnabled",fc.enabled),d("filterCleanDays",fc.intervalDays),b("chlorineEnabled",ch.enabled),d("chlorineDays",ch.intervalDays));
  if(ok){mqttManager.publishState();server.send(200,"application/json",maintenanceManager.toJson());}else server.send(500,"application/json","{\"ok\":false}");
}
void handleMaintenanceDone() {
  if(!server.hasArg("item")){server.send(400,"application/json","{\"ok\":false,\"error\":\"Onderdeel ontbreekt\"}");return;}
  if(!timeManager.isSynchronized()){server.send(409,"application/json","{\"ok\":false,\"error\":\"Tijd is nog niet gesynchroniseerd\"}");return;}
  if(!maintenanceManager.markDone(server.arg("item"))){server.send(400,"application/json","{\"ok\":false,\"error\":\"Onbekend onderdeel\"}");return;}
  mqttManager.publishState();server.send(200,"application/json",maintenanceManager.toJson());
}

void handleRegionalSettingsGet() {
  const auto& cfg = settingsManager.regional();

  String json;
  json.reserve(320);
  json = "{";
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
  String json;
  json.reserve(320);
  json = "{";
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

  String json;
  json.reserve(700);
  json = "{";
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
  if (server.hasArg("heaterHours") || server.hasArg("filterHours") || server.hasArg("bubblesHours") || server.hasArg("jetsHours")) {
    if (!energyManager.restoreTotals(server.arg("heaterHours").toDouble(), server.arg("filterHours").toDouble(), server.arg("bubblesHours").toDouble(), server.arg("jetsHours").toDouble())) {
      server.send(500,"application/json","{\"ok\":false,\"error\":\"Energietellers herstellen mislukt\"}"); return;
    }
  }
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

  String json;
  json.reserve(768);
  json = "{";
  json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"mqttConnected\":" + String(mqttManager.isConnected() ? "true" : "false");
  json += ",\"timeSynchronized\":" + String(timeManager.isSynchronized() ? "true" : "false");
  json += ",\"spaConnected\":" + String(spa.connected ? "true" : "false");
  json += ",\"spaDataValid\":" + String(spa.dataValid ? "true" : "false");
  json += ",\"freeHeap\":" + String(freeHeap);
  json += ",\"minimumObservedHeap\":" + String(minimumObservedHeap == UINT32_MAX ? freeHeap : minimumObservedHeap);
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
  json += ",\"otaLastError\":\"" + escapeJson(String(lastOtaError)) + "\"";
  json += ",\"mqttLastError\":" + String(mqttManager.lastErrorCode());
  json += ",\"mqttReconnectCount\":" + String(mqttManager.reconnectCount());
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
  server.on("/api/maintenance", HTTP_GET, handleMaintenanceGet);
  server.on("/api/maintenance", HTTP_POST, handleMaintenancePost);
  server.on("/api/maintenance/done", HTTP_POST, handleMaintenanceDone);
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
  server.on("/api/backup/file", HTTP_GET, handleBackupFileGet);
  server.on("/api/backup/file", HTTP_POST, handleBackupFilePost);


  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Webserver gestart");
  Serial.println("WebSocket gestart op poort 81");
}

bool webAppFilesystemUpdateActive() {
  return filesystemUpdateInProgress;
}

void webAppLoop() {
  const uint32_t currentHeap = ESP.getFreeHeap();
  if (currentHeap < minimumObservedHeap) minimumObservedHeap = currentHeap;

  server.handleClient();

  if (!filesystemUpdateInProgress) {
    webSocket.loop();
  }

  if (otaRestartPending &&
      static_cast<long>(millis() - otaRestartAt) >= 0) {
    ESP.restart();
  }

  if (!filesystemUpdateInProgress && millis() - lastBroadcast > 1000) {
    lastBroadcast = millis();
    webAppBroadcast();
  }
}
