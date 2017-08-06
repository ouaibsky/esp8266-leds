#include <FS.h>           //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>  //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h> //for LED status
#include <RemoteDebug.h>
#include "FastLED.h"
#include "Lights.h"
//#include <BlynkSimpleEsp8266.h>

#include "Common.h"

void tick()
{
  //toggle state
  int state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}
#include <RemoteDebug.h>


void setupRemoteDebug() {
  debug.begin(host_name); // Initiaze the telnet server - HOST_NAME is the used in MDNS.begin
  debug.setResetCmdEnabled(true); // Enable the reset command
  debug.setSerialEnabled(true);
  // debug.showTime(true); // To show time
  // debug.showProfiler(true); // To show profiler - time between messages of Debug
}


//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.print("Entered config mode, APIP: ");
  Serial.print(WiFi.softAPIP());
  Serial.print(", PortalSSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  Serial.println();
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void readConfig() {
  if (SPIFFS.begin()) {
    Serial.printf("Reading config file: %s\n", CONFIG_FILE_NAME);
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
    if (configFile) {
      size_t size = configFile.size();
      if ( size == 0 ) {
        Serial.printf("Config File Empty: %s\n", CONFIG_FILE_NAME);
      } else {
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        //StaticJsonBuffer<44000> jsonBuffer;
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          strcpy(host_name, json["hostName"]);
          strcpy(blynk_token, json["blynkToken"]);
        }
      }
    } else {
      Serial.printf("Config file not found!: %s\n", CONFIG_FILE_NAME);
    }
  }
  else {
    Serial.println("Failed to mount FS");
  }
}

void saveConfig() {
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.printf("Saving config: %s\n", CONFIG_FILE_NAME);
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["hostName"] = host_name;
    json["blynkToken"] = blynk_token;

    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "w");
    if (!configFile) {
      Serial.printf("Failed to open config file for writing: %s\n", CONFIG_FILE_NAME);
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
  }
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void handleNotFound() {
  debug.printf("URI not found: %s\n", server->uri().c_str()); // OR
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

void handleRoot() {
//  server->send(200, "text/plain", "hello from esp8266!");
}

void setupWebServer() {
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));
  //server->on("/", handleRoot);

  server->on("/reset", []() {
    wifiManager.resetSettings();
    delay(1000);
    ESP.reset();
    server->send(200, "text/plain", "reset and reconfig");
  });

  server->on("/hello", []() {
    server->send(200, "text/plain", "hello from esp8266!");
  });

  server->onNotFound(handleNotFound);
  server->begin();
  Serial.print("HTTP server started, IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  delay(100);

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  ticker.attach(0.5, tick);         // start ticker with 0.5 because we start in AP mode and try to connect

  //SPIFFS.format();                // clean FS, for testing
  readConfig();
  WiFi.hostname(host_name);

  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 34);
  WiFiManagerParameter custom_hostname("Hostname", "Unique hostname on your network", host_name, 69);

  Serial.println("");
  Serial.printf("Hostname: %s\n", host_name);
  Serial.printf("BlynkID: %s\n", blynk_token);

  //WiFiManager
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_hostname);
  //wifiManager.resetSettings();  // reset settings - for testing

  wifiManager.setAPCallback(configModeCallback);          //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setSaveConfigCallback(saveConfigCallback);  //set config save notify callback
  wifiManager.setTimeout(600);                            // 10 minutes to enter data and then Wemos resets to try again.

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(AP_NAME)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(host_name, custom_hostname.getValue());
  Serial.printf("Hostname: %s\n", host_name);
  Serial.printf("Blynk ID: %s\n", blynk_token);
  WiFi.hostname(host_name);

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();

  saveConfig();
  setupWebServer();
  setupRemoteDebug();

  if (!MDNS.begin(host_name)) {
    Serial.println("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("telnet", "tcp", 23);

  ArduinoOTA.setHostname(host_name); // on donne une petit nom a notre module
  ArduinoOTA.begin(); // initialisation de l'OTA
  setupFastLed();

  digitalWrite(LED_BUILTIN, HIGH);  //keep LED on
}//end setup

long t1 = millis();

void loop() {
  server->handleClient();
  ArduinoOTA.handle();
  debug.handle();
  loopLeds();
}//end loop



