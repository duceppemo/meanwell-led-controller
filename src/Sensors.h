#pragma once
#include <Arduino.h>

// DS18B20 on 1-Wire (heatsink / canopy temperature) plus values that can be
// posted by an external system (Home Assistant, a separate climate sensor...).
void  sensorsBegin();
void  sensorsLoop();
float sensorTemperature();   // NAN when unavailable (local first, then external if fresh)
float sensorHumidity();      // NAN unless posted externally
bool  sensorLocalPresent();
void  sensorsSetExternal(float tempC, float humidity);
