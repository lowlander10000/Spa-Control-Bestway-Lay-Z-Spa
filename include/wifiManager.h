#pragma once

#include <Arduino.h>

enum class WifiScanState : uint8_t {
  Idle,
  Scanning,
  Ready,
  Failed
};

enum class WifiConnectionState : uint8_t {
  Idle,
  Connecting,
  Connected,
  Failed
};

// Initialisatie en niet-blokkerende verwerking.
void wifiBegin();
void wifiLoop();

// Asynchrone WiFi-scan.
bool wifiStartScan();
WifiScanState wifiGetScanState();
String wifiGetScanResultsJson();
void wifiClearScanResults();

// Verbinden met een nieuw WiFi-netwerk.
bool wifiConnect(
  const String& ssid,
  const String& password,
  bool useDhcp = true,
  const String& ip = "",
  const String& gateway = "",
  const String& subnet = "",
  const String& dns1 = "",
  const String& dns2 = ""
);
WifiConnectionState wifiGetConnectionState();
String wifiConnectionStateText();
String wifiLastError();

// Actuele stationinformatie.
bool wifiIsConnected();
String wifiCurrentSsid();
String wifiIpAddress();
int wifiSignalStrength();
String wifiMacAddress();
String wifiHostname();

// Actuele accesspointinformatie.
bool wifiIsAccessPointActive();
String wifiAccessPointSsid();
String wifiAccessPointIpAddress();

// Uniforme JSON-status voor de instellingenpagina.
String wifiGetStatusJson();

// Verwijdert opgeslagen WiFi-gegevens en activeert de setupmodus.
bool wifiForgetCredentials();
