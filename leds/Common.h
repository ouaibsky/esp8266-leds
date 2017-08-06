#ifndef __INC_COMMON_CUBE_H
#define __INC_COMMON_CUBE_H

#include <EEPROM.h>

#define AP_NAME "WELCOME_ICROCO_LEDS"
#define LED_BUILTIN 2   // looks like esp-12E as builtin led on pin 2 (not pin 1 as for ESP-01)
#define CONFIG_FILE_NAME "/config.json"

char host_name[68] = "icrocoled1";
char blynk_token[33] = "YOUR_BLYNK_TOKEN";
Ticker ticker;
bool shouldSaveConfig = false;  // flag for saving data
std::unique_ptr<ESP8266WebServer> server;
WiFiManager wifiManager;
RemoteDebug debug;

#endif

