#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266SSDP.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "AutoPID.h"
//#include "AutoPID.cpp"
#define WIFI_CONNECT_TIMEOUT 30

#define RELAY_PIN D7
#define PULSEWIDTH 5000

//temperature sensor libraries and variables
#include <OneWire.h>
#include <DallasTemperature.h>
#define TEMP_SENSOR_PIN D4
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensors(&oneWire);
#define TEMP_READ_DELAY 800
double temperature, setTemp;
unsigned long timeAtTemp;
bool relayControl, powerOn;
AutoPIDRelay myPID(&temperature, &setTemp, &relayControl, 5000, .12, .0003, 0);

const char *metaRefreshStr = "<head><meta http-equiv=\"refresh\" content=\"1; url=/\" /></head><body><a href=\"/\">redirecting...</a></body>";

unsigned long lastTempUpdate;
void updateTemperature() {
  if ((millis() - lastTempUpdate) > TEMP_READ_DELAY) {
    temperature = temperatureSensors.getTempFByIndex(0);
    lastTempUpdate = millis();
    temperatureSensors.requestTemperatures();
  }
} //void updateTemperature

ESP8266WebServer server(80);
int scannedNetworks, scanssid;
DNSServer dnsServer;
IPAddress apIP(192, 168, 1, 1);

//code from fsbrowser example, consolidated.
bool handleFileRead(String path) {
  Serial.println("handlefileread" + path);
  if (path.endsWith("/")) path += "index.htm";
  String contentType;
  if (path.endsWith(".htm") || path.endsWith(".html")) contentType = "text/html";
  else if (path.endsWith(".css")) contentType = "text/css";
  else if (path.endsWith(".js")) contentType = "application/javascript";
  else if (path.endsWith(".png")) contentType = "image/png";
  else if (path.endsWith(".gif")) contentType = "image/gif";
  else if (path.endsWith(".jpg")) contentType = "image/jpeg";
  else if (path.endsWith(".ico")) contentType = "image/x-icon";
  else if (path.endsWith(".xml")) contentType = "text/xml";
  else if (path.endsWith(".pdf")) contentType = "application/x-pdf";
  else if (path.endsWith(".zip")) contentType = "application/x-zip";
  else if (path.endsWith(".gz")) contentType = "application/x-gzip";
  else if (path.endsWith(".json")) contentType = "application/json";
  else contentType = "text/plain";
  String pathGz = path + ".gz";
  if (SPIFFS.exists(pathGz) || SPIFFS.exists(path)) {
    Serial.println("sending file " + path);
    File file = SPIFFS.open(SPIFFS.exists(pathGz) ? pathGz : path, "r");
    if (server.hasArg("download"))
      server.sendHeader("Content-Disposition", " attachment;");
    if (server.uri().indexOf("nocache") < 0)
      server.sendHeader("Cache-Control", " max-age=172800");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  } //if SPIFFS.exists
  return false;
} //bool handleFileRead

void networkSetup() {
  //attempt to connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin();
  Serial.println("WiFi.begin"); /////////////////////////////////////////////////
  unsigned long connectTime = millis();
  while ((millis() - connectTime) < 1000 * WIFI_CONNECT_TIMEOUT && WiFi.status() != WL_CONNECTED)
    delay(10);
  if (WiFi.status() != WL_CONNECTED) { //if timed out, switch to AP mode
    Serial.println("WIFI_AP"); ////////////////////////////////////////////////////
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("Sous Vide WiFi");
  } //if
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start((byte)53, "*", apIP); //used for captive portal in AP mode
  //allows serving of files from SPIFFS
  SPIFFS.begin();

  server.onNotFound([]() {
    if (!handleFileRead(server.uri())) server.send(302, "text/html", metaRefreshStr);
  }); //server.onNotFound

  //run a scan for wifi networks on setup
  scannedNetworks = WiFi.scanNetworks();



//  server.on("/wifi/list.json", []() {
//    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
//    server.send(200, "application/json", "");
//    String cssid = WiFi.SSID();
//    cssid.replace("\"", "\\\"");
//    server.sendContent("{" + ((WiFi.status() == WL_CONNECTED) ? ("\"connected\":\"" + cssid + "\",") : "") + "\"networks\":[\n");
//    for (int i = 0; i < scannedNetworks; i++) {
//      String ssid = WiFi.SSID(i);
//      ssid.replace("\"", "\\\"");
//      server.sendContent("{\"ssid\":\"" + ssid + "\"," + "\"signal\":" + (WiFi.RSSI(i) + 100) + "," + "\"encrypted\":" + ((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "false" : "true") + "}" + ((i < scannedNetworks - 1) ? ",\n" : "]}"));
//    } //for
//    server.client().stop();
//  }); //server.on /wifi/list.json

  server.on("/wifi/rescan", []() {
    scannedNetworks = WiFi.scanNetworks();
    server.send(200, "text/html", "");
  });

  server.on("/wifi/info", []() {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    if (server.hasArg("n")) {
      int n = server.arg("n").toInt();
      if (n >= scannedNetworks) return;
      json["n"] = n;
      json["ssid"] = WiFi.SSID(n);
      json["signal"] = WiFi.RSSI(n) + 100;
      json["encrypted"] = !(WiFi.encryptionType(n) == ENC_TYPE_NONE);
    } else {
      json["networks"] = scannedNetworks;
      if(WiFi.status() == WL_CONNECTED) json["connected"] = WiFi.SSID();
    }
    char jsonchar[200];
    json.printTo(jsonchar);
    server.send(200, "application/json", jsonchar);
  });

  server.on("/wifi/wps", []() {
    server.send(200, "text/html", "WPS");
    WiFi.mode(WIFI_STA);
    WiFi.beginWPSConfig();
    delay(100);
    ESP.restart();
  }); //server.on /wifi/wps

  server.on("/wifi/connect", []() {
    if (server.hasArg("ssid") || server.hasArg("n"))
    { //connects to specified wifi network, then reboots
      WiFi.mode(WIFI_STA);
      String ssid = server.hasArg("n") ? WiFi.SSID(server.arg("n").toInt()): server.arg("ssid");
      (server.hasArg("encrypted") && server.arg("encrypted").equals("true")) ? WiFi.begin(ssid.c_str(), server.arg("pw").c_str()) : WiFi.begin(ssid.c_str());
      delay(100);
      ESP.restart();
    } //if
  }); //server.on /wifi/connect

  //handles commands from webpage, sends live data in JSON format
  server.on("/io", []() {
    Serial.println("server.on /io");////////////////////////
    if (server.hasArg("setTemp")) {
      powerOn = true;
      setTemp = server.arg("setTemp").toFloat();
    } //if
    if (server.hasArg("powerOff")) {
      powerOn = false;
    } //if

    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["temperature"] = temperature;
    json["setTemp"] = setTemp;
    json["power"] = myPID.getPulseValue();
    json["running"] = powerOn;
    json["upTime"] = ((timeAtTemp) ? (millis() - timeAtTemp) : 0);
    JsonArray& errors = json.createNestedArray("errors");
    if (temperature < -190) errors.add("sensor fault");
    char jsonchar[200];
    json.printTo(jsonchar);
    server.send(200, "application/json", jsonchar);

  }); //server.on io

  //SSDP makes device visible on windows network
  server.on("/description.xml", HTTP_GET, [&]() {
    SSDP.schema(server.client());
  });
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName("Sous Vide (" + WiFi.localIP().toString() + ")");
  SSDP.setURL("/");
  SSDP.begin();
  SSDP.setDeviceType("upnp:rootdevice");

  MDNS.begin("sousvide");
  MDNS.addService("http", "tcp", 80);
} //void networkSetup

void setup() {
  Serial.begin(115200); //for terminal debugging

  //set up temperature sensors and relay output
  temperatureSensors.begin();
  temperatureSensors.requestTemperatures();
  myPID.setBangBang(4);
  myPID.setTimeStep(4000);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  networkSetup();

  server.begin();
  Serial.println("setup complete.");
} //void setup

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  updateTemperature();
  if (powerOn) {
    myPID.run();
    digitalWrite(RELAY_PIN, !relayControl);
    if (myPID.atSetPoint(2)) {
      if (!timeAtTemp)
        timeAtTemp = millis();
    } else {
      timeAtTemp = 0;
    }
  } else { //!powerOn
    timeAtTemp = 0;
    myPID.stop();
    digitalWrite(RELAY_PIN, HIGH);
  } //endif
  //Serial.println(millis());
} //void loop
