INTEX SPA - WIFI INSTELLINGEN BACKEND

Deze frontend verwacht drie routes in src/webApp.cpp:

  GET  /api/wifi/status
  GET  /api/wifi/scan
  POST /api/wifi/connect

Voeg bovenaan webApp.cpp toe:

  #include "wifiManager.h"

Zorg dat deze functies BOVEN webAppBegin() staan:

void handleWifiScan() {
  server.send(200, "application/json", wifiScanJson());
}

void handleWifiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "application/json",
                "{\"ok\":false,\"error\":\"SSID ontbreekt\"}");
    return;
  }

  const String ssid = server.arg("ssid");
  const String password = server.arg("password");
  const bool success = wifiConnect(ssid, password);

  String response = "{";
  response += "\"ok\":";
  response += success ? "true" : "false";
  response += ",";
  response += "\"ip\":\"" + wifiIpAddress() + "\"";
  response += "}";

  server.send(success ? 200 : 500, "application/json", response);
}

void handleWifiStatus() {
  String response = "{";
  response += "\"connected\":";
  response += wifiIsConnected() ? "true" : "false";
  response += ",";
  response += "\"ssid\":\"" + wifiCurrentSsid() + "\",";
  response += "\"ip\":\"" + wifiIpAddress() + "\",";
  response += "\"rssi\":" + String(wifiSignalStrength());
  response += "}";

  server.send(200, "application/json", response);
}

Voeg binnen webAppBegin() toe:

  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);

Upload daarna:

  Build
  Build Filesystem Image
  Upload Filesystem Image
  Upload

De dashboardvormgeving is behouden. De instellingenpagina en WiFi-modal
worden zonder pagina-herlaad geopend.
