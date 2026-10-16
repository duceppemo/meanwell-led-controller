#pragma once
#include <Arduino.h>

// Async HTTP server: static web UI from LittleFS (/www), captive portal while in
// AP mode, JSON REST API under /api, OTA upload. Handlers run in the async TCP task,
// so anything that touches hardware or the filesystem is queued and executed by
// webLoop() from the main loop.
void webBegin();
void webLoop();
void webRequestFactoryReset();   // also used by the button
