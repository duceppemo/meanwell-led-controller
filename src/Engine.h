#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "pins.h"

enum Mode : uint8_t { MODE_AUTO = 0, MODE_MANUAL, MODE_OFF, MODE_PREVIEW };

// The engine turns "what time is it + which profile + effects + protections"
// into one target level per channel, then slews the outputs toward it.
void engineBegin();
void engineLoop();
void engineSetMode(Mode m, uint32_t timeoutS = 0);    // timeout: return to AUTO afterwards (feed / maintenance)
void engineSetManual(const float* levels, uint8_t n); // levels 0..100, also switches to MANUAL
void engineStartPreview(uint16_t seconds);            // play the whole day in `seconds`
void engineTriggerStorm(uint16_t seconds);
void engineStopStorm();
void engineStatusJson(JsonObject o);
void engineConfigChanged();                           // re-read cfg into the simulators / relay pin
bool engineRelayOn();
Mode engineMode();
