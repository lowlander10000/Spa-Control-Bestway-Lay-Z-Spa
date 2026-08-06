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

  void setDefaults();
};

extern SettingsManager settingsManager;
