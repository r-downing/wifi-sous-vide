/*
 * EasySSDP library
 * Ryan Downing
 * v 0.0.0
 */
#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266SSDP.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

namespace EasySSDP {

void begin(ESP8266WebServer &server, const char* deviceName="ESP8266") {

  //SSDP makes device visible on windows network
  server.on("/description.xml", HTTP_GET, [&]() {
    SSDP.schema(server.client());
  });
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(deviceName);
  SSDP.setModelName("esp8266");

  SSDP.setURL("/");
  SSDP.begin();
  SSDP.setDeviceType("upnp:rootdevice");
} //setup

} //namespace

