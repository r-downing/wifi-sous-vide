#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>


#ifndef PersistentWiFiManager_h
#define PersistentWiFiManager_h

#include "Arduino.h"

#define WIFI_CONNECT_TIMEOUT 30

class PersistentWiFiManager
{
  public:
    /*
      PersistentWiFiManager(ESP8266WebServer &server DNSServer &dnsServer) {
        _server = server;
        _dnsServer = dnsServer;
      }*/


    static void begin(ESP8266WebServer &_server, DNSServer &_dnsServer);


    static bool attemptConnection(String ssid, String pass, ESP8266WebServer &_server, DNSServer &_dnsServer);

    static void setupWiFiHandlers(ESP8266WebServer &_server, DNSServer &_dnsServer);


  private:
    PersistentWiFiManager() {}

};

#endif
