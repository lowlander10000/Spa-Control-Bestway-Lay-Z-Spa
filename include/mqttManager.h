#pragma once

#include <Arduino.h>

class MqttManager {
public:
  void begin();
  void loop();

  bool isConnected() const;

  void publishState();
  void publishAvailability(bool online);


  int lastErrorCode() const;
  uint32_t reconnectCount() const;

private:
  bool connected_ = false;
  unsigned long lastReconnectAttempt_ = 0;
  unsigned long lastPublishAt_ = 0;
  int lastErrorCode_ = 0;
  uint32_t reconnectCount_ = 0;

  void connect();
  void handleMessage(char* topic, byte* payload, unsigned int length);

};

extern MqttManager mqttManager;
