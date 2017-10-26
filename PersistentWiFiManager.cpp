#include "PersistentWiFiManager.h"

    




bool PersistentWiFiManager::attemptConnection(String ssid, String pass) {

  //attempt to connect to wifi
  WiFi.mode(WIFI_STA);
  if (ssid.length()) {
    if (pass.length()) WiFi.begin(ssid.c_str(), pass.c_str());
    else WiFi.begin(ssid.c_str());
  } else {
    WiFi.begin();
  }
  //Serial.println("WiFi.begin"); /////////////////////////////////////////////////
  IPAddress apIP(192, 168, 1, 1);
  unsigned long connectTime = millis();
  //while ((millis() - connectTime) < 1000 * WIFI_CONNECT_TIMEOUT && WiFi.status() != WL_CONNECTED)
  while (WiFi.status() != WL_CONNECT_FAILED && WiFi.status() != WL_CONNECTED && (millis() - connectTime) < 1000 * WIFI_CONNECT_TIMEOUT)
    delay(10);
  if (WiFi.status() != WL_CONNECTED) { //if timed out, switch to AP mode
    //Serial.println("WIFI_AP"); ////////////////////////////////////////////////////
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("ESP8266");
  } //if
  dnsServerP()->setErrorReplyCode(DNSReplyCode::NoError);
  dnsServerP()->start((byte)53, "*", apIP); //used for captive portal in AP mode
}


void PersistentWiFiManager::setupWiFiHandlers() {

  serverP()->on("/wifi/list", [] () {
    //scan for wifi networks
    int n = WiFi.scanNetworks();

    //build array of indices
    int ix[n];
    for (int i = 0; i < n; i++) ix[i] = i;

    //sort by signal strength
    for (int i = 0; i < n; i++) for (int j = 1; j < n - i; j++) if (WiFi.RSSI(ix[j]) > WiFi.RSSI(ix[j - 1])) std::swap(ix[j], ix[j - 1]);
    //remove duplicates
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (WiFi.SSID(ix[i]).equals(WiFi.SSID(ix[j]))) ix[j] = -1;

    //build plain text string of wifi info
    //format [signal%]:[encrypted 0 or 1]:SSID
    String s = "";
    for (int i = 0; i < n && s.length() < 2000; i++) { //check s.length to limit memory usage
      if (ix[i] != -1) {
        s += String(i ? "\n" : "") + ((constrain(WiFi.RSSI(ix[i]), -100, -50) + 100) * 2)
             + ":" + ((WiFi.encryptionType(ix[i]) == ENC_TYPE_NONE) ? 0 : 1)
             + ":" + WiFi.SSID(ix[i]);
      }
    }

    //send string to client
    serverP()->send(200, "text/plain", s);
  }); //server.on /wifi/list

  serverP()->on("/wifi/wps", []() {
    serverP()->send(200, "text/html", "attempting WPS");
    WiFi.mode(WIFI_STA);
    WiFi.beginWPSConfig();
    delay(100);
    if (WiFi.status() != WL_CONNECTED) {
      attemptConnection("", "");
    }
  }); //server.on /wifi/wps

  serverP()->on("/wifi/connect", []() {
    serverP()->send(200, "text/html", "attempting to connect...");
    attemptConnection(serverP()->arg("n"), serverP()->arg("p"));
  }); //server.on /wifi/connect

} //void setupWiFiHandlers

/*
void PersistentWiFiManager::setServers(ESP8266WebServer &server, DNSServer &dnsServer) {
  serverP() = server;
  dnsServerP() = dnsServer;
}*/

void PersistentWiFiManager::begin() {
  setupWiFiHandlers();
  attemptConnection("", "");
}


