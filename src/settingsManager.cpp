#include "settingsManager.h"

#include <LittleFS.h>

namespace {
  String escapeJson(const String& value) {
    String escaped;
    escaped.reserve(value.length() + 8);

    for (size_t i = 0; i < value.length(); i++) {
      const char character = value[i];

      switch (character) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += character; break;
      }
    }

    return escaped;
  }

  int findJsonValueStart(const String& json, const String& key) {
    const String pattern = "\"" + key + "\"";
    const int keyPosition = json.indexOf(pattern);
    if (keyPosition < 0) return -1;

    const int colonPosition = json.indexOf(':', keyPosition + pattern.length());
    if (colonPosition < 0) return -1;

    int valuePosition = colonPosition + 1;
    while (
      valuePosition < static_cast<int>(json.length()) &&
      isspace(json[valuePosition])
    ) {
      valuePosition++;
    }

    return valuePosition;
  }

  String readJsonString(
    const String& json,
    const String& key,
    const String& fallback
  ) {
    int position = findJsonValueStart(json, key);
    if (
      position < 0 ||
      position >= static_cast<int>(json.length()) ||
      json[position] != '"'
    ) {
      return fallback;
    }

    position++;
    String value;
    bool escaped = false;

    for (int i = position; i < static_cast<int>(json.length()); i++) {
      const char character = json[i];

      if (escaped) {
        switch (character) {
          case 'n': value += '\n'; break;
          case 'r': value += '\r'; break;
          case 't': value += '\t'; break;
          default: value += character; break;
        }
        escaped = false;
        continue;
      }

      if (character == '\\') {
        escaped = true;
        continue;
      }

      if (character == '"') return value;
      value += character;
    }

    return fallback;
  }

  bool readJsonBool(
    const String& json,
    const String& key,
    bool fallback
  ) {
    const int position = findJsonValueStart(json, key);
    if (position < 0) return fallback;
    if (json.startsWith("true", position)) return true;
    if (json.startsWith("false", position)) return false;
    return fallback;
  }

  uint16_t readJsonUInt16(
    const String& json,
    const String& key,
    uint16_t fallback
  ) {
    const int position = findJsonValueStart(json, key);
    if (position < 0) return fallback;

    String number;
    for (int i = position; i < static_cast<int>(json.length()); i++) {
      const char character = json[i];
      if (!isdigit(character)) break;
      number += character;
    }

    if (number.isEmpty()) return fallback;

    const unsigned long parsed = number.toInt();
    if (parsed == 0 || parsed > 65535) return fallback;
    return static_cast<uint16_t>(parsed);
  }
  float readJsonFloat(const String& json, const String& key, float fallback) {
    const int position = findJsonValueStart(json, key);
    if (position < 0) return fallback;
    String number;
    for (int i=position;i<(int)json.length();i++) {
      char c=json[i];
      if (!(isdigit(c)||c=='.'||c=='-')) break;
      number += c;
    }
    return number.isEmpty() ? fallback : number.toFloat();
  }

}

SettingsManager settingsManager;

bool SettingsManager::begin() {
  setDefaults();

  if (!LittleFS.exists(SETTINGS_FILE)) {
    Serial.println("Instellingenbestand ontbreekt, standaardwaarden actief");
    return save();
  }

  return load();
}

const MqttSettings& SettingsManager::mqtt() const { return mqttSettings_; }
MqttSettings& SettingsManager::mqtt() { return mqttSettings_; }
const RegionalSettings& SettingsManager::regional() const { return regionalSettings_; }
RegionalSettings& SettingsManager::regional() { return regionalSettings_; }
const EnergySettings& SettingsManager::energy() const { return energySettings_; }
EnergySettings& SettingsManager::energy() { return energySettings_; }
uint32_t SettingsManager::revision() const { return revision_; }

bool SettingsManager::load() {
  File file = LittleFS.open(SETTINGS_FILE, "r");
  if (!file) {
    Serial.println("Instellingenbestand openen mislukt");
    setDefaults();
    return false;
  }

  const String json = file.readString();
  file.close();

  if (json.isEmpty()) {
    Serial.println("Instellingenbestand is leeg");
    setDefaults();
    return false;
  }

  mqttSettings_.enabled = readJsonBool(json, "enabled", mqttSettings_.enabled);
  mqttSettings_.host = readJsonString(json, "host", mqttSettings_.host);
  mqttSettings_.port = readJsonUInt16(json, "port", mqttSettings_.port);
  mqttSettings_.username = readJsonString(json, "username", mqttSettings_.username);
  mqttSettings_.password = readJsonString(json, "password", mqttSettings_.password);
  mqttSettings_.clientId = readJsonString(json, "clientId", mqttSettings_.clientId);
  mqttSettings_.baseTopic = readJsonString(json, "baseTopic", mqttSettings_.baseTopic);
  mqttSettings_.homeAssistantDiscovery = readJsonBool(
    json,
    "homeAssistantDiscovery",
    mqttSettings_.homeAssistantDiscovery
  );
  mqttSettings_.publishTemperature = readJsonBool(json, "publishTemperature", mqttSettings_.publishTemperature);
  mqttSettings_.publishTarget = readJsonBool(json, "publishTarget", mqttSettings_.publishTarget);
  mqttSettings_.publishPower = readJsonBool(json, "publishPower", mqttSettings_.publishPower);
  mqttSettings_.publishHeater = readJsonBool(json, "publishHeater", mqttSettings_.publishHeater);
  mqttSettings_.publishHeatingActive = readJsonBool(json, "publishHeatingActive", mqttSettings_.publishHeatingActive);
  mqttSettings_.publishFilter = readJsonBool(json, "publishFilter", mqttSettings_.publishFilter);
  mqttSettings_.publishBubbles = readJsonBool(json, "publishBubbles", mqttSettings_.publishBubbles);
  mqttSettings_.publishJets = readJsonBool(json, "publishJets", mqttSettings_.publishJets);
  mqttSettings_.publishLocked = readJsonBool(json, "publishLocked", mqttSettings_.publishLocked);
  mqttSettings_.publishConnected = readJsonBool(json, "publishConnected", mqttSettings_.publishConnected);
  mqttSettings_.publishReady = readJsonBool(json, "publishReady", mqttSettings_.publishReady);
  mqttSettings_.publishRssi = readJsonBool(json, "publishRssi", mqttSettings_.publishRssi);
  mqttSettings_.publishHeap = readJsonBool(json, "publishHeap", mqttSettings_.publishHeap);
  mqttSettings_.publishUptime = readJsonBool(json, "publishUptime", mqttSettings_.publishUptime);
  mqttSettings_.publishFirmware = readJsonBool(json, "publishFirmware", mqttSettings_.publishFirmware);
  mqttSettings_.publishIp = readJsonBool(json, "publishIp", mqttSettings_.publishIp);
  mqttSettings_.publishJson = readJsonBool(json, "publishJson", mqttSettings_.publishJson);
  mqttSettings_.publishMaintenance = readJsonBool(json, "publishMaintenance", mqttSettings_.publishMaintenance);
  mqttSettings_.topicTemperature = readJsonString(json, "topicTemperature", mqttSettings_.topicTemperature);
  mqttSettings_.topicTarget = readJsonString(json, "topicTarget", mqttSettings_.topicTarget);
  mqttSettings_.topicPower = readJsonString(json, "topicPower", mqttSettings_.topicPower);
  mqttSettings_.topicHeater = readJsonString(json, "topicHeater", mqttSettings_.topicHeater);
  mqttSettings_.topicHeatingActive = readJsonString(json, "topicHeatingActive", mqttSettings_.topicHeatingActive);
  mqttSettings_.topicFilter = readJsonString(json, "topicFilter", mqttSettings_.topicFilter);
  mqttSettings_.topicBubbles = readJsonString(json, "topicBubbles", mqttSettings_.topicBubbles);
  mqttSettings_.topicJets = readJsonString(json, "topicJets", mqttSettings_.topicJets);
  mqttSettings_.topicLocked = readJsonString(json, "topicLocked", mqttSettings_.topicLocked);
  mqttSettings_.topicConnected = readJsonString(json, "topicConnected", mqttSettings_.topicConnected);
  mqttSettings_.topicReady = readJsonString(json, "topicReady", mqttSettings_.topicReady);
  mqttSettings_.topicRssi = readJsonString(json, "topicRssi", mqttSettings_.topicRssi);
  mqttSettings_.topicHeap = readJsonString(json, "topicHeap", mqttSettings_.topicHeap);
  mqttSettings_.topicUptime = readJsonString(json, "topicUptime", mqttSettings_.topicUptime);
  mqttSettings_.topicFirmware = readJsonString(json, "topicFirmware", mqttSettings_.topicFirmware);
  mqttSettings_.topicIp = readJsonString(json, "topicIp", mqttSettings_.topicIp);
  mqttSettings_.topicJson = readJsonString(json, "topicJson", mqttSettings_.topicJson);
  mqttSettings_.topicMaintenance = readJsonString(json, "topicMaintenance", mqttSettings_.topicMaintenance);

  regionalSettings_.language = readJsonString(
    json,
    "language",
    regionalSettings_.language
  );
  regionalSettings_.timeZone = readJsonString(
    json,
    "timeZone",
    regionalSettings_.timeZone
  );
  regionalSettings_.use24HourClock = readJsonBool(
    json,
    "use24HourClock",
    regionalSettings_.use24HourClock
  );
  regionalSettings_.dateFormat = readJsonString(
    json,
    "dateFormat",
    regionalSettings_.dateFormat
  );
  regionalSettings_.temperatureUnit = temperatureUnitFromString(
    readJsonString(
      json,
      "temperatureUnit",
      temperatureUnitToString(regionalSettings_.temperatureUnit)
    ),
    regionalSettings_.temperatureUnit
  );

  energySettings_.heaterWatts = readJsonUInt16(json, "heaterWatts", energySettings_.heaterWatts);
  energySettings_.filterWatts = readJsonUInt16(json, "filterWatts", energySettings_.filterWatts);
  energySettings_.bubblesWatts = readJsonUInt16(json, "bubblesWatts", energySettings_.bubblesWatts);
  energySettings_.jetsWatts = readJsonUInt16(json, "jetsWatts", energySettings_.jetsWatts);
  energySettings_.pricePerKwh = readJsonFloat(json, "pricePerKwh", energySettings_.pricePerKwh);
  energySettings_.currency = readJsonString(json, "currency", energySettings_.currency);

  ++revision_;
  Serial.println("Instellingen geladen");
  return true;
}

bool SettingsManager::save() {
  File file = LittleFS.open(SETTINGS_FILE, "w");
  if (!file) {
    Serial.println("Instellingenbestand schrijven mislukt");
    return false;
  }

  String json;
  json.reserve(2600);
  json = "{\n";
  json += "  \"enabled\": " + String(mqttSettings_.enabled ? "true" : "false") + ",\n";
  json += "  \"host\": \"" + escapeJson(mqttSettings_.host) + "\",\n";
  json += "  \"port\": " + String(mqttSettings_.port) + ",\n";
  json += "  \"username\": \"" + escapeJson(mqttSettings_.username) + "\",\n";
  json += "  \"password\": \"" + escapeJson(mqttSettings_.password) + "\",\n";
  json += "  \"clientId\": \"" + escapeJson(mqttSettings_.clientId) + "\",\n";
  json += "  \"baseTopic\": \"" + escapeJson(mqttSettings_.baseTopic) + "\",\n";
  json += "  \"homeAssistantDiscovery\": " + String(
    mqttSettings_.homeAssistantDiscovery ? "true" : "false"
  ) + ",\n";
  json += "  \"publishTemperature\": " + String(mqttSettings_.publishTemperature ? "true" : "false") + ",\n";
  json += "  \"publishTarget\": " + String(mqttSettings_.publishTarget ? "true" : "false") + ",\n";
  json += "  \"publishPower\": " + String(mqttSettings_.publishPower ? "true" : "false") + ",\n";
  json += "  \"publishHeater\": " + String(mqttSettings_.publishHeater ? "true" : "false") + ",\n";
  json += "  \"publishHeatingActive\": " + String(mqttSettings_.publishHeatingActive ? "true" : "false") + ",\n";
  json += "  \"publishFilter\": " + String(mqttSettings_.publishFilter ? "true" : "false") + ",\n";
  json += "  \"publishBubbles\": " + String(mqttSettings_.publishBubbles ? "true" : "false") + ",\n";
  json += "  \"publishJets\": " + String(mqttSettings_.publishJets ? "true" : "false") + ",\n";
  json += "  \"publishLocked\": " + String(mqttSettings_.publishLocked ? "true" : "false") + ",\n";
  json += "  \"publishConnected\": " + String(mqttSettings_.publishConnected ? "true" : "false") + ",\n";
  json += "  \"publishReady\": " + String(mqttSettings_.publishReady ? "true" : "false") + ",\n";
  json += "  \"publishRssi\": " + String(mqttSettings_.publishRssi ? "true" : "false") + ",\n";
  json += "  \"publishHeap\": " + String(mqttSettings_.publishHeap ? "true" : "false") + ",\n";
  json += "  \"publishUptime\": " + String(mqttSettings_.publishUptime ? "true" : "false") + ",\n";
  json += "  \"publishFirmware\": " + String(mqttSettings_.publishFirmware ? "true" : "false") + ",\n";
  json += "  \"publishIp\": " + String(mqttSettings_.publishIp ? "true" : "false") + ",\n";
  json += "  \"publishJson\": " + String(mqttSettings_.publishJson ? "true" : "false") + ",\n";
  json += "  \"publishMaintenance\": " + String(mqttSettings_.publishMaintenance ? "true" : "false") + ",\n";
  json += "  \"topicTemperature\": \"" + escapeJson(mqttSettings_.topicTemperature) + "\",\n";
  json += "  \"topicTarget\": \"" + escapeJson(mqttSettings_.topicTarget) + "\",\n";
  json += "  \"topicPower\": \"" + escapeJson(mqttSettings_.topicPower) + "\",\n";
  json += "  \"topicHeater\": \"" + escapeJson(mqttSettings_.topicHeater) + "\",\n";
  json += "  \"topicHeatingActive\": \"" + escapeJson(mqttSettings_.topicHeatingActive) + "\",\n";
  json += "  \"topicFilter\": \"" + escapeJson(mqttSettings_.topicFilter) + "\",\n";
  json += "  \"topicBubbles\": \"" + escapeJson(mqttSettings_.topicBubbles) + "\",\n";
  json += "  \"topicJets\": \"" + escapeJson(mqttSettings_.topicJets) + "\",\n";
  json += "  \"topicLocked\": \"" + escapeJson(mqttSettings_.topicLocked) + "\",\n";
  json += "  \"topicConnected\": \"" + escapeJson(mqttSettings_.topicConnected) + "\",\n";
  json += "  \"topicReady\": \"" + escapeJson(mqttSettings_.topicReady) + "\",\n";
  json += "  \"topicRssi\": \"" + escapeJson(mqttSettings_.topicRssi) + "\",\n";
  json += "  \"topicHeap\": \"" + escapeJson(mqttSettings_.topicHeap) + "\",\n";
  json += "  \"topicUptime\": \"" + escapeJson(mqttSettings_.topicUptime) + "\",\n";
  json += "  \"topicFirmware\": \"" + escapeJson(mqttSettings_.topicFirmware) + "\",\n";
  json += "  \"topicIp\": \"" + escapeJson(mqttSettings_.topicIp) + "\",\n";
  json += "  \"topicJson\": \"" + escapeJson(mqttSettings_.topicJson) + "\",\n";
  json += "  \"topicMaintenance\": \"" + escapeJson(mqttSettings_.topicMaintenance) + "\",\n";
  json += "  \"language\": \"" + escapeJson(regionalSettings_.language) + "\",\n";
  json += "  \"timeZone\": \"" + escapeJson(regionalSettings_.timeZone) + "\",\n";
  json += "  \"use24HourClock\": " + String(
    regionalSettings_.use24HourClock ? "true" : "false"
  ) + ",\n";
  json += "  \"dateFormat\": \"" + escapeJson(regionalSettings_.dateFormat) + "\",\n";
  json += "  \"temperatureUnit\": \"";
  json += temperatureUnitToString(regionalSettings_.temperatureUnit);
  json += "\",\n";
  json += "  \"heaterWatts\": " + String(energySettings_.heaterWatts) + ",\n";
  json += "  \"filterWatts\": " + String(energySettings_.filterWatts) + ",\n";
  json += "  \"bubblesWatts\": " + String(energySettings_.bubblesWatts) + ",\n";
  json += "  \"jetsWatts\": " + String(energySettings_.jetsWatts) + ",\n";
  json += "  \"pricePerKwh\": " + String(energySettings_.pricePerKwh, 3) + ",\n";
  json += "  \"currency\": \"" + escapeJson(energySettings_.currency) + "\"\n}";

  const size_t written = file.print(json);
  file.close();

  if (written != json.length()) {
    Serial.println("Instellingenbestand onvolledig geschreven");
    return false;
  }

  ++revision_;
  Serial.println("Instellingen opgeslagen");
  return true;
}

bool SettingsManager::reset() {
  setDefaults();
  if (LittleFS.exists(SETTINGS_FILE)) LittleFS.remove(SETTINGS_FILE);
  return save();
}

const char* SettingsManager::temperatureUnitToString(TemperatureUnit unit) {
  return unit == TemperatureUnit::Fahrenheit ? "Fahrenheit" : "Celsius";
}

TemperatureUnit SettingsManager::temperatureUnitFromString(
  const String& value,
  TemperatureUnit fallback
) {
  String normalized = value;
  normalized.trim();
  normalized.toLowerCase();

  if (normalized == "fahrenheit" || normalized == "f") {
    return TemperatureUnit::Fahrenheit;
  }
  if (normalized == "celsius" || normalized == "c") {
    return TemperatureUnit::Celsius;
  }
  return fallback;
}

void SettingsManager::setDefaults() {
  mqttSettings_ = MqttSettings{};
  regionalSettings_ = RegionalSettings{};
  energySettings_ = EnergySettings{};
}
