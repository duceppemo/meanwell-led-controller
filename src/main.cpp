// Mean Well LED driver controller: PWM dimming with sunrise / sunset schedules,
// effects (lunar, clouds, lightning), thermal protection, web UI and REST API.
#include <Arduino.h>
#include <LittleFS.h>
#include "pins.h"
#include "Config.h"
#include "Schedule.h"
#include "Channels.h"
#include "TimeKeeper.h"
#include "Sensors.h"
#include "Engine.h"
#include "WifiManager.h"
#include "WebUi.h"
#include "BleProv.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static uint32_t s_pressedAt = 0;
static bool s_apTriggered = false;

// BOOT button: hold 5 s = start the access point, hold 15 s = factory reset
static void buttonLoop() {
  bool pressed = digitalRead(PIN_BUTTON) == LOW;
  uint32_t now = millis();
  if (pressed) {
    if (s_pressedAt == 0) { s_pressedAt = now; s_apTriggered = false; }
    uint32_t held = now - s_pressedAt;
    if (held > 5000 && !s_apTriggered) { s_apTriggered = true; wifiStartAP(); log_w("button: AP started"); }
    if (held > 15000) { log_w("button: factory reset"); webRequestFactoryReset(); s_pressedAt = 0; }
  } else {
    s_pressedAt = 0;
  }
}

// Status LED: AP mode = fast blink, connecting = slow blink, connected = short blip every 3 s
static void ledLoop() {
#if PIN_STATUS_LED >= 0
  uint32_t t = millis();
  bool on;
  if (wifiIsAP())            on = (t % 300) < 150;
  else if (!wifiConnected()) on = (t % 1000) < 500;
  else                       on = (t % 3000) < 40;
  if (s_pressedAt) on = ((t - s_pressedAt) % 200) < 100;   // feedback while the button is held
  digitalWrite(PIN_STATUS_LED, on ? HIGH : LOW);
#endif
}

void setup() {
  Serial.begin(115200);
  delay(100);
  log_i("Mean Well LED controller %s", FW_VERSION);

  pinMode(PIN_BUTTON, INPUT_PULLUP);
#if PIN_STATUS_LED >= 0
  pinMode(PIN_STATUS_LED, OUTPUT);
#endif

  if (!LittleFS.begin(true)) log_e("LittleFS mount failed");
  configLoad();
  profilesLoad();

  channelsBegin();     // outputs at 0 % as early as possible
  engineBegin();
  timeBegin();
  sensorsBegin();
  wifiBegin();
  webBegin();
  bleBegin();
  log_i("ready: %u channels, profile '%s'", cfg.numChannels, profiles.p[profiles.active].name);
}

void loop() {
  wifiLoop();
  timeLoop();
  sensorsLoop();
  engineLoop();
  webLoop();
  bleLoop();
  buttonLoop();
  ledLoop();
  delay(2);
}
