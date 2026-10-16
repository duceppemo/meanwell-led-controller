#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Boot: try saved STA credentials for 20 s. On failure (or no credentials) start
// the "LEDctrl-xxxx" access point with a captive portal. While in AP mode the
// STA connection is retried every 2 min; once connected the AP is closed after 3 min.
void wifiBegin();
void wifiLoop();
bool wifiIsAP();
bool wifiConnected();
void wifiStartAP();                                   // force AP (button / API)
void wifiConnect(const char* ssid, const char* pass); // save + reconnect
void wifiStatusJson(JsonObject o);
void wifiScanJson(JsonArray a);                       // async scan; returns last result
String wifiApSsid();
