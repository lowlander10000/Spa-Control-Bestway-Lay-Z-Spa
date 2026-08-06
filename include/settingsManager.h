#pragma once

#include <Arduino.h>

enum class TemperatureUnit : uint8_t {
  Celsius = 0,
  Fahrenheit = 1
};

struct MqttSettings {
  bool enabled = false;
  String host = "";
  uint16_t port = 1883;
  String username = "";
  String password = "";
  String clientId = "LayZSpaController";
  String baseTopic = "layzspa";
  bool homeAssistantDiscovery = true;

  bool publishTemperature = true;
  bool publishTarget = true;
  bool publishPower = true;
  bool publishHeater = true;
  bool publishHeatingActive = true;
  bool publishFilter = true;
  bool publishBubbles = true;
  bool publishJets = true;
  bool publishLocked = true;
  bool publishConnected = true;
  bool publishReady = true;
  bool publishRssi = false;
  bool publishHeap = false;
  bool publishUptime = false;
  bool publishFirmware = false;
  bool publishIp = false;
  bool publishJson = true;
  bool publishMaintenance = true;

  String topicTemperature = "temperature";
  String topicTarget = "target/state";
  String topicPower = "power/state";
  String topicHeater = "heater/state";
  String topicHeatingActive = "heater/active";
  String topicFilter = "filter/state";
  String topicBubbles = "bubbles/state";
  String topicJets = "jets/state";
  String topicLocked = "lock/state";
  String topicConnected = "spa/connected";
  String topicReady = "spa/ready";
  String topicRssi = "system/rssi";
  String topicHeap = "system/free_heap";
  String topicUptime = "system/uptime";
  String topicFirmware = "system/firmware";
  String topicIp = "system/ip";
  String topicJson = "state";
  String topicMaintenance = "maintenance";
};

struct EnergySettings {
  uint16_t heaterWatts = 2200;
  uint16_t filterWatts = 60;
  uint16_t bubblesWatts = 800;
  uint16_t jetsWatts = 800;
  float pricePerKwh = 0.30f;
  String currency = "EUR";
};

struct RegionalSettings {
  String language = "nl";
  String timeZone = "Europe/Amsterdam";
  bool use24HourClock = true;
  String dateFormat = "DD-MM-YYYY";
  TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
};

class SettingsManager {
public:
  bool begin();

  const MqttSettings& mqtt() const;
  MqttSettings& mqtt();

  const RegionalSettings& regional() const;
  RegionalSettings& regional();

  const EnergySettings& energy() const;
  EnergySettings& energy();

  bool load();
  bool save();
  bool reset();
  uint32_t revision() const;

  static const char* temperatureUnitToString(TemperatureUnit unit);
  static TemperatureUnit temperatureUnitFromString(
    const String& value,
    TemperatureUnit fallback = TemperatureUnit::Celsius
  );

private:
  static constexpr const char* SETTINGS_FILE = "/settings.json";

  MqttSettings mqttSettings_;
  RegionalSettings regionalSettings_;
  EnergySettings energySettings_;
  uint32_t revision_ = 0;

  void setDefaults();
};

extern SettingsManager settingsManager;
