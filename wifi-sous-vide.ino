#define DEBUG

#ifdef DEBUG
#define DEBUG_START Serial.begin(115200)
#define DEBUG_PRINT(x) Serial.println(x)
#else
#define DEBUG_PRINT(x) 
#define DEBUG_START
#endif


#include <PersWiFiManager.h> //http://ryandowning.net/PersWiFiManager
#include <AutoPID.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266SSDP.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>
#include <SPIFFSReadServer.h> //http://ryandowning.net/SPIFFSReadServer
#include <EasySSDP.h> //http://ryandowning.net/EasySSDP

#define RELAY_PIN D7
#define PULSEWIDTH 5000

#define DEVICE_NAME "Sous Vide"

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

unsigned long lastTempUpdate;
void updateTemperature() {
  if ((millis() - lastTempUpdate) > TEMP_READ_DELAY) {
    temperature = temperatureSensors.getTempFByIndex(0);
    lastTempUpdate = millis();
    temperatureSensors.requestTemperatures();
  }
} //void updateTemperature

SPIFFSReadServer server(80);
int scannedNetworks, scanssid;
DNSServer dnsServer;
PersWiFiManager persWM(server, dnsServer);

void setup() {
  DEBUG_START; //for terminal debugging
  DEBUG_PRINT();

  //allows serving of files from SPIFFS
  SPIFFS.begin();

  //set up temperature sensors and relay output
  temperatureSensors.begin();
  temperatureSensors.requestTemperatures();
  myPID.setBangBang(4);
  myPID.setTimeStep(4000);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  persWM.setApCredentials(DEVICE_NAME);
  persWM.onConnect([](){
    EasySSDP::begin(server, DEVICE_NAME);
  });
  persWM.begin();

  //handles commands from webpage, sends live data in JSON format
  server.on("/io", []() {
    #ifdef DEBUG
    for(int i = 0; i<server.args(); i++){
      DEBUG_PRINT(server.argName(i)+":"+server.arg(i));
    }
    #endif
    DEBUG_PRINT("server.on /io");////////////////////////
    if (server.hasArg("setTemp")) {
      powerOn = true;
      setTemp = server.arg("setTemp").toFloat();
      DEBUG_PRINT(setTemp);
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

  server.on("/esp8266-project.json", [] () {
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();
    json["device_name"] = DEVICE_NAME;
    char jsonchar[200];
    json.printTo(jsonchar);
    server.send(200, "application/json", jsonchar);
  });


  server.begin();
  DEBUG_PRINT("setup complete.");
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
