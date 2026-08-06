#include "mqttManager.h"

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "settingsManager.h"
#include "spaInterface.h"
#include "spaState.h"

namespace {
  WiFiClient wifiClient;
  PubSubClient mqttClient(wifiClient);

  constexpr unsigned long RECONNECT_INTERVAL_MS = 10000;
  constexpr unsigned long PUBLISH_INTERVAL_MS = 5000;

  String activeConfigKey;

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
    const String& icon
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
               topic(objectId + "/state") +
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
               topic("temperature") +
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
               topic("target/state") +
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

    publishSwitchDiscovery(
      "heater",
      "Heater",
      "mdi:radiator"
    );

    publishSwitchDiscovery(
      "filter",
      "Filter",
      "mdi:water-pump"
    );

    publishSwitchDiscovery(
      "bubbles",
      "Bubbels",
      "mdi:chart-bubble"
    );

    publishSwitchDiscovery(
      "jets",
      "Jets",
      "mdi:waves"
    );

    payload = "{";
    payload += "\"name\":\"Spa verbinding\",";
    payload += "\"unique_id\":\"layzspa_connection\",";
    payload += "\"object_id\":\"layzspa_connection\",";
    payload += "\"device_class\":\"connectivity\",";
    payload += "\"state_topic\":\"" +
               topic("spa/connected") +
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

    Serial.println("Home Assistant discovery gepubliceerd");
  }

  String makeConfigKey() {
    const auto& cfg = settingsManager.mqtt();

    return
      String(cfg.enabled ? "1" : "0") + "|" +
      cfg.host + "|" +
      String(cfg.port) + "|" +
      cfg.username + "|" +
      cfg.clientId + "|" +
      cfg.baseTopic;
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

  activeConfigKey = makeConfigKey();
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
  const String currentConfigKey = makeConfigKey();

  if (currentConfigKey != activeConfigKey) {
    activeConfigKey = currentConfigKey;

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
    Serial.print(
      "MQTT verbinden mislukt, status: "
    );
    Serial.println(mqttClient.state());
    return;
  }

  connected_ = true;
  spa.mqttOnline = true;

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
}

void MqttManager::publishState() {
  if (!mqttClient.connected()) {
    return;
  }

  publishRetained(
    topic("temperature"),
    String(spa.temperature)
  );

  publishRetained(
    topic("target/state"),
    String(spa.targetTemperature)
  );

  publishRetained(
    topic("heater/state"),
    spa.heater ? "ON" : "OFF"
  );

  publishRetained(
    topic("filter/state"),
    spa.filter ? "ON" : "OFF"
  );

  publishRetained(
    topic("bubbles/state"),
    spa.bubbles ? "ON" : "OFF"
  );

  publishRetained(
    topic("jets/state"),
    spa.jets ? "ON" : "OFF"
  );

  publishRetained(
    topic("spa/connected"),
    spa.connected ? "ON" : "OFF"
  );

  publishRetained(
    topic("state"),
    spa.toJson()
  );
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
  const String value =
    payloadToString(payload, length);

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
