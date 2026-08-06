#include "mqttManager.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "settingsManager.h"
#include "spaInterface.h"
#include "spaState.h"
#include "maintenanceManager.h"
#include "eventLog.h"

namespace {
  WiFiClient wifiClient;
  PubSubClient mqttClient(wifiClient);

  constexpr unsigned long RECONNECT_INTERVAL_MS = 10000;
  constexpr unsigned long PUBLISH_INTERVAL_MS = 5000;

  uint32_t activeSettingsRevision = 0;

  String topic(const String& suffix) {
    const auto& cfg = settingsManager.mqtt();

    String base = cfg.baseTopic;
    base.trim();

    while (base.endsWith("/")) {
      base.remove(base.length() - 1);
    }

    if (base.isEmpty()) {
      base = "layzspa";
    }

    return base + "/" + suffix;
  }

  String discoveryTopic(
    const String& component,
    const String& objectId
  ) {
    return "homeassistant/" +
           component +
           "/layzspa/" +
           objectId +
           "/config";
  }

  String deviceJson() {
    return
      "\"device\":{"
        "\"identifiers\":[\"layzspa_controller\"],"
        "\"name\":\"Bestway Lay-Z-Spa\","
        "\"manufacturer\":\"Intex\","
        "\"model\":\"Lay-Z-Spa\","
        "\"sw_version\":\"v48\""
      "}";
  }

  String availabilityJson() {
    return
      "\"availability_topic\":\"" +
      topic("availability") +
      "\","
      "\"payload_available\":\"online\","
      "\"payload_not_available\":\"offline\"";
  }

  String payloadToString(
    byte* payload,
    unsigned int length
  ) {
    String value;
    value.reserve(length);

    for (unsigned int i = 0; i < length; i++) {
      value += static_cast<char>(payload[i]);
    }

    value.trim();
    value.toLowerCase();
    return value;
  }

  bool isOnCommand(const String& value) {
    return value == "on" ||
           value == "1" ||
           value == "true";
  }

  bool isOffCommand(const String& value) {
    return value == "off" ||
           value == "0" ||
           value == "false";
  }

  bool publishRetained(
    const String& publishTopic,
    const String& payload
  ) {
    return mqttClient.publish(
      publishTopic.c_str(),
      payload.c_str(),
      true
    );
  }

  void publishDiscoveryMessage(
    const String& component,
    const String& objectId,
    const String& payload
  ) {
    publishRetained(
      discoveryTopic(component, objectId),
      payload
    );
  }

  void publishSwitchDiscovery(
    const String& objectId,
    const String& name,
    const String& icon,
    const String& stateTopicSuffix
  ) {
    String payload;
    payload.reserve(700);

    payload = "{";
    payload += "\"name\":\"" + name + "\",";
    payload += "\"unique_id\":\"layzspa_" + objectId + "\",";
    payload += "\"object_id\":\"layzspa_" + objectId + "\",";
    payload += "\"icon\":\"" + icon + "\",";
    payload += "\"command_topic\":\"" +
               topic("command/" + objectId) +
               "\",";
    payload += "\"state_topic\":\"" +
               topic(stateTopicSuffix) +
               "\",";
    payload += "\"payload_on\":\"ON\",";
    payload += "\"payload_off\":\"OFF\",";
    payload += "\"state_on\":\"ON\",";
    payload += "\"state_off\":\"OFF\",";
    payload += availabilityJson() + ",";
    payload += deviceJson();
    payload += "}";

    publishDiscoveryMessage(
      "switch",
      objectId,
      payload
    );
  }

  void publishBinarySensorDiscovery(
    const String& objectId,
    const String& name,
    const String& icon,
    const String& stateTopicSuffix,
    const String& deviceClass = ""
  ) {
    String payload = "{";
    payload += "\"name\":\"" + name + "\",";
    payload += "\"unique_id\":\"layzspa_" + objectId + "\",";
    payload += "\"object_id\":\"layzspa_" + objectId + "\",";
    if (!icon.isEmpty()) payload += "\"icon\":\"" + icon + "\",";
    if (!deviceClass.isEmpty()) payload += "\"device_class\":\"" + deviceClass + "\",";
    payload += "\"state_topic\":\"" + topic(stateTopicSuffix) + "\",";
    payload += "\"payload_on\":\"ON\",\"payload_off\":\"OFF\",";
    payload += availabilityJson() + "," + deviceJson() + "}";
    publishDiscoveryMessage("binary_sensor", objectId, payload);
  }

  void publishSensorDiscovery(
    const String& objectId,
    const String& name,
    const String& icon,
    const String& stateTopicSuffix,
    const String& unit = "",
    const String& deviceClass = "",
    const String& stateClass = ""
  ) {
    String payload = "{";
    payload += "\"name\":\"" + name + "\",";
    payload += "\"unique_id\":\"layzspa_" + objectId + "\",";
    payload += "\"object_id\":\"layzspa_" + objectId + "\",";
    if (!icon.isEmpty()) payload += "\"icon\":\"" + icon + "\",";
    if (!unit.isEmpty()) payload += "\"unit_of_measurement\":\"" + unit + "\",";
    if (!deviceClass.isEmpty()) payload += "\"device_class\":\"" + deviceClass + "\",";
    if (!stateClass.isEmpty()) payload += "\"state_class\":\"" + stateClass + "\",";
    payload += "\"state_topic\":\"" + topic(stateTopicSuffix) + "\",";
    payload += availabilityJson() + "," + deviceJson() + "}";
    publishDiscoveryMessage("sensor", objectId, payload);
  }

  void publishDiscovery() {
    const auto& cfg = settingsManager.mqtt();

    if (!cfg.homeAssistantDiscovery) {
      return;
    }

    String payload;
    payload.reserve(800);

    payload = "{";
    payload += "\"name\":\"Watertemperatuur\",";
    payload += "\"unique_id\":\"layzspa_temperature\",";
    payload += "\"object_id\":\"layzspa_temperature\",";
    payload += "\"device_class\":\"temperature\",";
    payload += "\"state_class\":\"measurement\",";
    payload += "\"state_topic\":\"" +
               topic(cfg.topicTemperature) +
               "\",";
    payload += "\"unit_of_measurement\":\"°C\",";
    payload += availabilityJson() + ",";
    payload += deviceJson();
    payload += "}";

    publishDiscoveryMessage(
      "sensor",
      "temperature",
      payload
    );

    payload = "{";
    payload += "\"name\":\"Doeltemperatuur\",";
    payload += "\"unique_id\":\"layzspa_target_temperature\",";
    payload += "\"object_id\":\"layzspa_target_temperature\",";
    payload += "\"device_class\":\"temperature\",";
    payload += "\"command_topic\":\"" +
               topic("command/target") +
               "\",";
    payload += "\"state_topic\":\"" +
               topic(cfg.topicTarget) +
               "\",";
    payload += "\"min\":20,";
    payload += "\"max\":40,";
    payload += "\"step\":1,";
    payload += "\"mode\":\"slider\",";
    payload += "\"unit_of_measurement\":\"°C\",";
    payload += availabilityJson() + ",";
    payload += deviceJson();
    payload += "}";

    publishDiscoveryMessage(
      "number",
      "target_temperature",
      payload
    );

    if (cfg.publishHeater) publishSwitchDiscovery(
      "heater",
      "Heater",
      "mdi:radiator",
      cfg.topicHeater
    );

    if (cfg.publishFilter) publishSwitchDiscovery(
      "filter",
      "Filter",
      "mdi:water-pump",
      cfg.topicFilter
    );

    if (cfg.publishBubbles) publishSwitchDiscovery(
      "bubbles",
      "Bubbels",
      "mdi:chart-bubble",
      cfg.topicBubbles
    );

    if (cfg.publishJets) publishSwitchDiscovery(
      "jets",
      "Jets",
      "mdi:waves",
      cfg.topicJets
    );

    payload = "{";
    payload += "\"name\":\"Spa verbinding\",";
    payload += "\"unique_id\":\"layzspa_connection\",";
    payload += "\"object_id\":\"layzspa_connection\",";
    payload += "\"device_class\":\"connectivity\",";
    payload += "\"state_topic\":\"" +
               topic(cfg.topicConnected) +
               "\",";
    payload += "\"payload_on\":\"ON\",";
    payload += "\"payload_off\":\"OFF\",";
    payload += availabilityJson() + ",";
    payload += deviceJson();
    payload += "}";

    publishDiscoveryMessage(
      "binary_sensor",
      "connection",
      payload
    );

    if (cfg.publishPower) publishBinarySensorDiscovery("power", "Power", "mdi:power", cfg.topicPower, "power");
    if (cfg.publishHeatingActive) publishBinarySensorDiscovery("heating_active", "Verwarmt actief", "mdi:fire", cfg.topicHeatingActive, "heat");
    if (cfg.publishLocked) publishBinarySensorDiscovery("locked", "Vergrendeling", "mdi:lock", cfg.topicLocked, "lock");
    if (cfg.publishReady) publishBinarySensorDiscovery("ready", "Spa gereed", "mdi:hot-tub", cfg.topicReady);
    if (cfg.publishRssi) publishSensorDiscovery("wifi_rssi", "WiFi-signaal", "mdi:wifi", cfg.topicRssi, "dBm", "signal_strength", "measurement");
    if (cfg.publishHeap) publishSensorDiscovery("free_heap", "Vrij geheugen", "mdi:memory", cfg.topicHeap, "B", "data_size", "measurement");
    if (cfg.publishUptime) publishSensorDiscovery("uptime", "Uptime", "mdi:timer-outline", cfg.topicUptime, "s", "duration", "total_increasing");
    if (cfg.publishFirmware) publishSensorDiscovery("firmware", "Firmwareversie", "mdi:chip", cfg.topicFirmware);
    if (cfg.publishIp) publishSensorDiscovery("ip_address", "IP-adres", "mdi:ip-network", cfg.topicIp);

    if (cfg.publishMaintenance) {
      publishSensorDiscovery("maintenance_status", "Onderhoudsstatus", "mdi:tools", cfg.topicMaintenance + "/status");
      publishSensorDiscovery("filter_replace_days", "Filter vervangen over", "mdi:filter", cfg.topicMaintenance + "/filter_replace/days_remaining", "d");
      publishBinarySensorDiscovery("filter_replace_due", "Filter vervangen nodig", "mdi:filter-alert", cfg.topicMaintenance + "/filter_replace/due", "problem");
      publishSensorDiscovery("filter_clean_days", "Filter schoonmaken over", "mdi:filter-check", cfg.topicMaintenance + "/filter_clean/days_remaining", "d");
      publishBinarySensorDiscovery("filter_clean_due", "Filter schoonmaken nodig", "mdi:filter-alert", cfg.topicMaintenance + "/filter_clean/due", "problem");
      publishSensorDiscovery("chlorine_days", "Chloor toevoegen over", "mdi:water-plus", cfg.topicMaintenance + "/chlorine/days_remaining", "d");
      publishBinarySensorDiscovery("chlorine_due", "Chloor toevoegen nodig", "mdi:alert-circle", cfg.topicMaintenance + "/chlorine/due", "problem");
    }

    Serial.println("Home Assistant discovery gepubliceerd");
  }

  void applyBrokerSettings() {
    const auto& cfg = settingsManager.mqtt();
    mqttClient.setServer(cfg.host.c_str(), cfg.port);
  }
}

MqttManager mqttManager;

void MqttManager::begin() {
  connected_ = false;
  lastReconnectAttempt_ = 0;
  lastPublishAt_ = 0;
  lastErrorCode_ = 0;
  reconnectCount_ = 0;
  spa.mqttOnline = false;

  mqttClient.setBufferSize(1024);

  mqttClient.setCallback([](
    char* receivedTopic,
    byte* payload,
    unsigned int length
  ) {
    mqttManager.handleMessage(
      receivedTopic,
      payload,
      length
    );
  });

  activeSettingsRevision = settingsManager.revision();
  applyBrokerSettings();

  const auto& cfg = settingsManager.mqtt();

  if (!cfg.enabled || cfg.host.isEmpty()) {
    Serial.println(
      "MQTT uitgeschakeld: broker niet ingesteld"
    );
    return;
  }

  Serial.print("MQTT broker: ");
  Serial.print(cfg.host);
  Serial.print(":");
  Serial.println(cfg.port);
}

void MqttManager::loop() {
  const auto& cfg = settingsManager.mqtt();
  const uint32_t currentSettingsRevision = settingsManager.revision();

  if (currentSettingsRevision != activeSettingsRevision) {
    activeSettingsRevision = currentSettingsRevision;

    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }

    connected_ = false;
    spa.mqttOnline = false;
    lastReconnectAttempt_ = 0;

    applyBrokerSettings();

    Serial.println(
      "MQTT-instellingen gewijzigd"
    );
  }

  if (!cfg.enabled || cfg.host.isEmpty()) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }

    connected_ = false;
    spa.mqttOnline = false;
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }

    connected_ = false;
    spa.mqttOnline = false;
    return;
  }

  if (!mqttClient.connected()) {
    connected_ = false;
    spa.mqttOnline = false;

    if (
      millis() - lastReconnectAttempt_ >=
      RECONNECT_INTERVAL_MS
    ) {
      lastReconnectAttempt_ = millis();
      connect();
    }

    return;
  }

  mqttClient.loop();

  connected_ = mqttClient.connected();
  spa.mqttOnline = connected_;

  if (
    connected_ &&
    millis() - lastPublishAt_ >=
    PUBLISH_INTERVAL_MS
  ) {
    lastPublishAt_ = millis();
    publishState();
  }
}

bool MqttManager::isConnected() const {
  return connected_;
}

int MqttManager::lastErrorCode() const {
  return lastErrorCode_;
}

uint32_t MqttManager::reconnectCount() const {
  return reconnectCount_;
}

void MqttManager::connect() {
  const auto& cfg = settingsManager.mqtt();

  if (
    WiFi.status() != WL_CONNECTED ||
    !cfg.enabled ||
    cfg.host.isEmpty()
  ) {
    return;
  }

  applyBrokerSettings();

  const String availabilityTopic =
    topic("availability");

  bool connected = false;

  if (cfg.username.isEmpty()) {
    connected = mqttClient.connect(
      cfg.clientId.c_str(),
      availabilityTopic.c_str(),
      0,
      true,
      "offline"
    );
  } else {
    connected = mqttClient.connect(
      cfg.clientId.c_str(),
      cfg.username.c_str(),
      cfg.password.c_str(),
      availabilityTopic.c_str(),
      0,
      true,
      "offline"
    );
  }

  if (!connected) {
    lastErrorCode_ = mqttClient.state();
    reconnectCount_++;
    const String message = String("MQTT verbinden mislukt, status: ") + lastErrorCode_;
    Serial.println(message);
    eventLog.warning(message);
    return;
  }

  connected_ = true;
  spa.mqttOnline = true;
  lastErrorCode_ = 0;

  mqttClient.subscribe(
    topic("command/heater").c_str()
  );
  mqttClient.subscribe(
    topic("command/filter").c_str()
  );
  mqttClient.subscribe(
    topic("command/bubbles").c_str()
  );
  mqttClient.subscribe(
    topic("command/jets").c_str()
  );
  mqttClient.subscribe(
    topic("command/target").c_str()
  );

  publishAvailability(true);
  publishDiscovery();
  publishState();

  Serial.println("MQTT verbonden");
  eventLog.info("MQTT verbonden");
}

void MqttManager::publishState() {
  if (!mqttClient.connected()) return;
  const auto& cfg=settingsManager.mqtt();
  auto pub=[&](bool enabled,const String& suffix,const String& payload){if(enabled){String clean=suffix;clean.trim();if(!clean.isEmpty())publishRetained(topic(clean),payload);}};
  pub(cfg.publishTemperature,cfg.topicTemperature,String(spa.temperature));
  pub(cfg.publishTarget,cfg.topicTarget,String(spa.targetTemperature));
  pub(cfg.publishPower,cfg.topicPower,spa.power?"ON":"OFF");
  pub(cfg.publishHeater,cfg.topicHeater,spa.heater?"ON":"OFF");
  pub(cfg.publishHeatingActive,cfg.topicHeatingActive,spa.heaterActive?"ON":"OFF");
  pub(cfg.publishFilter,cfg.topicFilter,spa.filter?"ON":"OFF");
  pub(cfg.publishBubbles,cfg.topicBubbles,spa.bubbles?"ON":"OFF");
  pub(cfg.publishJets,cfg.topicJets,spa.jets?"ON":"OFF");
  pub(cfg.publishLocked,cfg.topicLocked,spa.locked?"ON":"OFF");
  pub(cfg.publishConnected,cfg.topicConnected,spa.connected?"ON":"OFF");
  const bool ready=spa.power && spa.connected && spa.dataValid && spa.temperature>=spa.targetTemperature;
  pub(cfg.publishReady,cfg.topicReady,ready?"ON":"OFF");
  pub(cfg.publishRssi,cfg.topicRssi,String(WiFi.RSSI()));
  pub(cfg.publishHeap,cfg.topicHeap,String(ESP.getFreeHeap()));
  pub(cfg.publishUptime,cfg.topicUptime,spa.uptime());
  pub(cfg.publishFirmware,cfg.topicFirmware,"3.2.0");
  pub(cfg.publishIp,cfg.topicIp,WiFi.localIP().toString());
  pub(cfg.publishJson,cfg.topicJson,spa.toJson());
  if(cfg.publishMaintenance){
    String root=cfg.topicMaintenance;root.trim();while(root.endsWith("/"))root.remove(root.length()-1);if(root.isEmpty())root="maintenance";
    auto pubItem=[&](const String& name,const MaintenanceItem& item){
      publishRetained(topic(root+"/"+name+"/due"),maintenanceManager.due(item)?"ON":"OFF");
      publishRetained(topic(root+"/"+name+"/days_remaining"),String(maintenanceManager.daysRemaining(item)));
      publishRetained(topic(root+"/"+name+"/last_date"),maintenanceManager.isoDate(item.lastDone));
      publishRetained(topic(root+"/"+name+"/next_date"),maintenanceManager.nextDate(item));
    };
    pubItem("filter_replace",maintenanceManager.filterReplace());
    pubItem("filter_clean",maintenanceManager.filterClean());
    pubItem("chlorine",maintenanceManager.chlorine());
    publishRetained(topic(root+"/status"),maintenanceManager.overallStatus());
  }
}

void MqttManager::publishAvailability(
  bool online
) {
  if (!mqttClient.connected()) {
    return;
  }

  publishRetained(
    topic("availability"),
    online ? "online" : "offline"
  );
}

void MqttManager::handleMessage(
  char* receivedTopic,
  byte* payload,
  unsigned int length
) {
  const String commandTopic = receivedTopic;
  const String value = payloadToString(payload, length);


  if (
    commandTopic ==
    topic("command/heater")
  ) {
    if (isOnCommand(value)) spaInterface.setHeater(true);
    else if (isOffCommand(value)) spaInterface.setHeater(false);
  } else if (
    commandTopic ==
    topic("command/filter")
  ) {
    if (isOnCommand(value)) spaInterface.setFilter(true);
    else if (isOffCommand(value)) spaInterface.setFilter(false);
  } else if (
    commandTopic ==
    topic("command/bubbles")
  ) {
    if (isOnCommand(value)) spaInterface.setBubbles(true);
    else if (isOffCommand(value)) spaInterface.setBubbles(false);
  } else if (
    commandTopic ==
    topic("command/jets")
  ) {
    if (isOnCommand(value)) spaInterface.setJets(true);
    else if (isOffCommand(value)) spaInterface.setJets(false);
  } else if (
    commandTopic ==
    topic("command/target")
  ) {
    const int requestedTarget = value.toInt();

    if (
      requestedTarget >= 20 &&
      requestedTarget <= 40
    ) {
      spaInterface.setTargetTemperature(requestedTarget);
    }
  }

  publishState();
}
