#include "wifiManager.h"

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <FS.h>

namespace {
  constexpr int EEPROM_SIZE = 512;
  constexpr int SSID_ADDRESS = 0;
  constexpr int PASSWORD_ADDRESS = 100;
  constexpr int MAX_SSID_LENGTH = 32;
  constexpr int MAX_PASSWORD_LENGTH = 64;
  constexpr int DHCP_ADDRESS = 170;
  constexpr int IP_ADDRESS = 171;
  constexpr int GATEWAY_ADDRESS = 187;
  constexpr int SUBNET_ADDRESS = 203;
  constexpr int DNS1_ADDRESS = 219;
  constexpr int DNS2_ADDRESS = 235;
  constexpr int MAX_IPV4_LENGTH = 16;

  constexpr unsigned long CONNECT_TIMEOUT_MS = 15000;
  constexpr unsigned long RECONNECT_INTERVAL_MS = 30000;

  const char* AP_SSID = "LayZSpa-Setup";
  const char* AP_PASSWORD = "12345678";
  const char* WIFI_HOSTNAME = "layzspa-controller";

  String savedSsid;
  String savedPassword;
  bool savedUseDhcp = true;
  String savedIp;
  String savedGateway;
  String savedSubnet;
  String savedDns1;
  String savedDns2;

  String pendingSsid;
  String pendingPassword;
  bool pendingUseDhcp = true;
  String pendingIp;
  String pendingGateway;
  String pendingSubnet;
  String pendingDns1;
  String pendingDns2;

  String lastError;

  WifiScanState scanState = WifiScanState::Idle;
  WifiConnectionState connectionState = WifiConnectionState::Idle;

  unsigned long connectStartedAt = 0;
  unsigned long lastReconnectAttempt = 0;

  bool accessPointActive = false;
  bool savePendingCredentials = false;

  void writeStringToEeprom(int address, int maxLength, const String& value) {
    for (int i = 0; i < maxLength; i++) {
      const uint8_t byteValue =
        i < static_cast<int>(value.length())
          ? static_cast<uint8_t>(value[i])
          : 0;

      EEPROM.write(address + i, byteValue);
    }
  }

  String readStringFromEeprom(int address, int maxLength) {
    String value;
    value.reserve(maxLength);

    for (int i = 0; i < maxLength; i++) {
      const uint8_t stored = EEPROM.read(address + i);

      if (stored == 0 || stored == 0xFF) {
        break;
      }

      value += static_cast<char>(stored);
    }

    return value;
  }

  bool parseIp(const String& value, IPAddress& address) {
    return !value.isEmpty() && address.fromString(value);
  }

  void saveCredentials(
    const String& ssid,
    const String& password,
    bool useDhcp,
    const String& ip,
    const String& gateway,
    const String& subnet,
    const String& dns1,
    const String& dns2
  ) {
    EEPROM.begin(EEPROM_SIZE);

    writeStringToEeprom(SSID_ADDRESS, MAX_SSID_LENGTH, ssid);
    writeStringToEeprom(PASSWORD_ADDRESS, MAX_PASSWORD_LENGTH, password);
    EEPROM.write(DHCP_ADDRESS, useDhcp ? 1 : 0);
    writeStringToEeprom(IP_ADDRESS, MAX_IPV4_LENGTH, ip);
    writeStringToEeprom(GATEWAY_ADDRESS, MAX_IPV4_LENGTH, gateway);
    writeStringToEeprom(SUBNET_ADDRESS, MAX_IPV4_LENGTH, subnet);
    writeStringToEeprom(DNS1_ADDRESS, MAX_IPV4_LENGTH, dns1);
    writeStringToEeprom(DNS2_ADDRESS, MAX_IPV4_LENGTH, dns2);

    EEPROM.commit();
    EEPROM.end();
  }

  void loadCredentials() {
    EEPROM.begin(EEPROM_SIZE);

    savedSsid = readStringFromEeprom(SSID_ADDRESS, MAX_SSID_LENGTH);
    savedPassword = readStringFromEeprom(PASSWORD_ADDRESS, MAX_PASSWORD_LENGTH);
    const uint8_t dhcpValue = EEPROM.read(DHCP_ADDRESS);
    savedUseDhcp = dhcpValue == 0xFF ? true : dhcpValue != 0;
    savedIp = readStringFromEeprom(IP_ADDRESS, MAX_IPV4_LENGTH);
    savedGateway = readStringFromEeprom(GATEWAY_ADDRESS, MAX_IPV4_LENGTH);
    savedSubnet = readStringFromEeprom(SUBNET_ADDRESS, MAX_IPV4_LENGTH);
    savedDns1 = readStringFromEeprom(DNS1_ADDRESS, MAX_IPV4_LENGTH);
    savedDns2 = readStringFromEeprom(DNS2_ADDRESS, MAX_IPV4_LENGTH);

    EEPROM.end();
  }

  String escapeJson(String value) {
    value.replace("\\", "\\\\");
    value.replace("\"", "\\\"");
    value.replace("\n", "\\n");
    value.replace("\r", "\\r");
    value.replace("\t", "\\t");
    return value;
  }

  void startAccessPoint() {
    if (accessPointActive) {
      return;
    }

    WiFi.mode(WIFI_AP_STA);

    if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
      Serial.println("Access Point starten mislukt");
      lastError = "Access Point starten mislukt";
      return;
    }

    accessPointActive = true;

    Serial.println("Access Point actief");
    Serial.print("SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
  }

  void stopAccessPoint() {
    if (!accessPointActive) {
      return;
    }

    WiFi.softAPdisconnect(true);
    accessPointActive = false;
    WiFi.mode(WIFI_STA);

    Serial.println("Access Point uitgeschakeld");
  }

  void beginConnection(
    const String& ssid,
    const String& password,
    bool saveAfterSuccess,
    bool useDhcp,
    const String& ip,
    const String& gateway,
    const String& subnet,
    const String& dns1,
    const String& dns2
  ) {
    pendingSsid = ssid;
    pendingPassword = password;
    pendingUseDhcp = useDhcp;
    pendingIp = ip;
    pendingGateway = gateway;
    pendingSubnet = subnet;
    pendingDns1 = dns1;
    pendingDns2 = dns2;
    savePendingCredentials = saveAfterSuccess;

    lastError = "";
    connectionState = WifiConnectionState::Connecting;
    connectStartedAt = millis();

    WiFi.mode(WIFI_AP_STA);
    WiFi.disconnect(false);

    if (pendingUseDhcp) {
      const IPAddress zero(0, 0, 0, 0);
      WiFi.config(zero, zero, zero);
    } else {
      IPAddress ipAddress;
      IPAddress gatewayAddress;
      IPAddress subnetAddress;
      IPAddress dns1Address;
      IPAddress dns2Address;

      if (!parseIp(pendingIp, ipAddress) ||
          !parseIp(pendingGateway, gatewayAddress) ||
          !parseIp(pendingSubnet, subnetAddress)) {
        connectionState = WifiConnectionState::Failed;
        lastError = "Ongeldige statische IP-configuratie";
        return;
      }

      if (!parseIp(pendingDns1, dns1Address)) {
        dns1Address = gatewayAddress;
      }
      if (!parseIp(pendingDns2, dns2Address)) {
        dns2Address = dns1Address;
      }

      if (!WiFi.config(ipAddress, gatewayAddress, subnetAddress, dns1Address, dns2Address)) {
        connectionState = WifiConnectionState::Failed;
        lastError = "Statisch IP instellen mislukt";
        return;
      }
    }

    WiFi.begin(pendingSsid.c_str(), pendingPassword.c_str());

    Serial.print("WiFi verbinden gestart: ");
    Serial.println(pendingSsid);
  }

  void handleConnectionState() {
    if (connectionState != WifiConnectionState::Connecting) {
      return;
    }

    const wl_status_t status = WiFi.status();

    if (status == WL_CONNECTED) {
      connectionState = WifiConnectionState::Connected;
      lastError = "";

      if (savePendingCredentials) {
        savedSsid = pendingSsid;
        savedPassword = pendingPassword;
        savedUseDhcp = pendingUseDhcp;
        savedIp = pendingIp;
        savedGateway = pendingGateway;
        savedSubnet = pendingSubnet;
        savedDns1 = pendingDns1;
        savedDns2 = pendingDns2;
        saveCredentials(
          savedSsid, savedPassword, savedUseDhcp, savedIp,
          savedGateway, savedSubnet, savedDns1, savedDns2
        );
      }

      savePendingCredentials = false;

      Serial.println("WiFi verbonden");
      Serial.print("SSID: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP-adres: ");
      Serial.println(WiFi.localIP());

      stopAccessPoint();
      return;
    }

    if (millis() - connectStartedAt < CONNECT_TIMEOUT_MS) {
      return;
    }

    connectionState = WifiConnectionState::Failed;
    savePendingCredentials = false;
    lastError = "Verbindingstime-out";

    Serial.println("WiFi verbinden mislukt");
    startAccessPoint();
  }

  void handleLostConnection() {
    if (connectionState == WifiConnectionState::Connecting) {
      return;
    }

    if (WiFi.status() == WL_CONNECTED) {
      connectionState = WifiConnectionState::Connected;
      return;
    }

    if (connectionState == WifiConnectionState::Connected) {
      Serial.println("WiFi-verbinding verloren");
      connectionState = WifiConnectionState::Idle;
      startAccessPoint();
    }

    if (savedSsid.isEmpty()) {
      return;
    }

    if (millis() - lastReconnectAttempt < RECONNECT_INTERVAL_MS) {
      return;
    }

    lastReconnectAttempt = millis();
    beginConnection(
      savedSsid, savedPassword, false, savedUseDhcp,
      savedIp, savedGateway, savedSubnet, savedDns1, savedDns2
    );
  }

  void handleScanState() {
    if (scanState != WifiScanState::Scanning) {
      return;
    }

    const int result = WiFi.scanComplete();

    if (result == WIFI_SCAN_RUNNING) {
      return;
    }

    if (result == WIFI_SCAN_FAILED) {
      scanState = WifiScanState::Failed;
      lastError = "WiFi-scan mislukt";
      return;
    }

    scanState = WifiScanState::Ready;
  }
}

void wifiBegin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.mode(WIFI_AP_STA);

  loadCredentials();
  startAccessPoint();

  if (!savedSsid.isEmpty()) {
    beginConnection(
      savedSsid, savedPassword, false, savedUseDhcp,
      savedIp, savedGateway, savedSubnet, savedDns1, savedDns2
    );
  } else {
    connectionState = WifiConnectionState::Idle;
  }
}

void wifiLoop() {
  handleScanState();
  handleConnectionState();
  handleLostConnection();
}

bool wifiStartScan() {
  if (scanState == WifiScanState::Scanning) {
    return true;
  }

  WiFi.scanDelete();

  const int result = WiFi.scanNetworks(true, true);

  if (result == WIFI_SCAN_FAILED) {
    scanState = WifiScanState::Failed;
    lastError = "WiFi-scan kon niet starten";
    return false;
  }

  scanState = WifiScanState::Scanning;
  lastError = "";

  Serial.println("Asynchrone WiFi-scan gestart");
  return true;
}

WifiScanState wifiGetScanState() {
  return scanState;
}

String wifiGetScanResultsJson() {
  if (scanState != WifiScanState::Ready) {
    return "[]";
  }

  const int networkCount = WiFi.scanComplete();

  if (networkCount < 0) {
    return "[]";
  }

  String json;
  json.reserve(64 + networkCount * 80);
  json = "[";

  bool first = true;

  for (int i = 0; i < networkCount; i++) {
    if (!first) {
      json += ",";
    }

    first = false;

    json += "{";
    json += "\"ssid\":\"";
    json += escapeJson(WiFi.SSID(i));
    json += "\",";
    json += "\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",";
    json += "\"encrypted\":";
    json += WiFi.encryptionType(i) == ENC_TYPE_NONE ? "false" : "true";
    json += "}";
  }

  json += "]";
  return json;
}

void wifiClearScanResults() {
  WiFi.scanDelete();
  scanState = WifiScanState::Idle;
}

bool wifiConnect(
  const String& ssid,
  const String& password,
  bool useDhcp,
  const String& ip,
  const String& gateway,
  const String& subnet,
  const String& dns1,
  const String& dns2
) {
  if (ssid.isEmpty() || ssid.length() > MAX_SSID_LENGTH) {
    lastError = "Ongeldige netwerknaam";
    connectionState = WifiConnectionState::Failed;
    return false;
  }

  if (password.length() > MAX_PASSWORD_LENGTH) {
    lastError = "Wachtwoord is te lang";
    connectionState = WifiConnectionState::Failed;
    return false;
  }

  if (!useDhcp) {
    IPAddress parsed;
    if (!parseIp(ip, parsed) || !parseIp(gateway, parsed) || !parseIp(subnet, parsed)) {
      lastError = "Vul een geldig IP-adres, gateway en subnet in";
      connectionState = WifiConnectionState::Failed;
      return false;
    }
  }

  beginConnection(
    ssid, password, true, useDhcp,
    ip, gateway, subnet, dns1, dns2
  );
  return true;
}

WifiConnectionState wifiGetConnectionState() {
  return connectionState;
}

String wifiConnectionStateText() {
  switch (connectionState) {
    case WifiConnectionState::Connecting:
      return "connecting";

    case WifiConnectionState::Connected:
      return "connected";

    case WifiConnectionState::Failed:
      return "failed";

    case WifiConnectionState::Idle:
    default:
      return "idle";
  }
}

String wifiLastError() {
  return lastError;
}

String wifiCurrentSsid() {
  if (WiFi.status() != WL_CONNECTED) {
    return pendingSsid;
  }

  return WiFi.SSID();
}

String wifiIpAddress() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }

  return WiFi.softAPIP().toString();
}

int wifiSignalStrength() {
  if (WiFi.status() != WL_CONNECTED) {
    return 0;
  }

  return WiFi.RSSI();
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}


String wifiMacAddress() {
  return WiFi.macAddress();
}

String wifiHostname() {
  const String hostname = WiFi.hostname();
  return hostname.isEmpty() ? String(WIFI_HOSTNAME) : hostname;
}

bool wifiIsAccessPointActive() {
  return accessPointActive;
}

String wifiAccessPointSsid() {
  return String(AP_SSID);
}

String wifiAccessPointIpAddress() {

    if (accessPointActive) {
        return WiFi.softAPIP().toString();
    }

    return "192.168.4.1";
}

String wifiGetStatusJson() {
  String json;
  json.reserve(320);

  json = "{";
  json += "\"connected\":";
  json += wifiIsConnected() ? "true" : "false";
  json += ",\"state\":\"";
  json += wifiConnectionStateText();
  json += "\",\"ssid\":\"";
  json += escapeJson(wifiCurrentSsid());
  json += "\",\"ip\":\"";
  json += wifiIsConnected() ? escapeJson(WiFi.localIP().toString()) : String("0.0.0.0");
  json += "\",\"rssi\":";
  json += String(wifiSignalStrength());
  json += ",\"mac\":\"";
  json += escapeJson(wifiMacAddress());
  json += "\",\"hostname\":\"";
  json += escapeJson(wifiHostname());

  json += "\",\"firmware\":\"v3.0.1";
  json += "\",\"cpu\":";
  json += String(ESP.getCpuFreqMHz());

  json += ",\"flash\":\"";
  json += String((float)ESP.getFlashChipRealSize()/1024.0/1024.0,1);
  json += " MB";

  FSInfo fs;
  LittleFS.info(fs);
  float usedKb=fs.usedBytes/1024.0;
  float totalKb=fs.totalBytes/1024.0;
  int percent=fs.totalBytes? (fs.usedBytes*100)/fs.totalBytes:0;

  json += "\",\"fs\":\"";
  json += String(usedKb,0);
  json += " kB / ";
  json += String(totalKb,0);
  json += " kB (";
  json += String(percent);
  json += "%)";

  json += "\",\"resetReason\":\"";
  json += escapeJson(ESP.getResetReason());

  json += "\",\"apMode\":";
  json += wifiIsAccessPointActive() ? "true" : "false";
  json += ",\"apSsid\":\"";
  json += escapeJson(wifiAccessPointSsid());
  json += "\",\"apIp\":\"";
  json += escapeJson(wifiAccessPointIpAddress());
  json += "\",\"useDhcp\":";
  json += savedUseDhcp ? "true" : "false";
  json += ",\"configuredIp\":\"";
  json += escapeJson(savedIp);
  json += "\",\"gateway\":\"";
  json += escapeJson(savedGateway);
  json += "\",\"subnet\":\"";
  json += escapeJson(savedSubnet);
  json += "\",\"dns1\":\"";
  json += escapeJson(savedDns1);
  json += "\",\"dns2\":\"";
  json += escapeJson(savedDns2);
  json += "\",\"error\":\"";
  json += escapeJson(wifiLastError());
  json += "\"}";

  return json;
}

bool wifiForgetCredentials() {
  EEPROM.begin(EEPROM_SIZE);

  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0xFF);
  }

  const bool committed = EEPROM.commit();
  EEPROM.end();

  if (!committed) {
    lastError = "WiFi-gegevens wissen mislukt";
    return false;
  }

  savedSsid = "";
  savedPassword = "";
  savedUseDhcp = true;
  savedIp = "";
  savedGateway = "";
  savedSubnet = "";
  savedDns1 = "";
  savedDns2 = "";
  pendingSsid = "";
  pendingPassword = "";
  savePendingCredentials = false;
  connectionState = WifiConnectionState::Idle;
  lastReconnectAttempt = millis();
  lastError = "";

  WiFi.disconnect(false);
  startAccessPoint();

  Serial.println("Opgeslagen WiFi-gegevens verwijderd");
  return true;
}
