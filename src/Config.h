#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "pins.h"

struct ChannelCfg {
  char  name[16];
  int8_t pin;
  bool  enabled;
  bool  invert;     // true when a direct NPN/MOSFET shorts DIM+ (GPIO high = 0 % output)
  float minPct;     // driver minimum usable level (XLG-AB ~8 %, HLG-C-B ~6 %); 0 % is still "off"
  float maxPct;     // cap, e.g. 80 % to derate a fixture
  float gamma;      // perceptual curve, 1.0 = linear
  bool  moon;       // channel participates in the lunar cycle
  bool  effects;    // channel participates in clouds / lightning
};

struct Config {
  // device
  char devName[32];
  char hostname[32];
  // wifi
  char ssid[33];
  char pass[65];
  char apPass[65];      // empty = open AP
  // time
  char tz[64];          // POSIX TZ string, e.g. "EST5EDT,M3.2.0,M11.1.0"
  char ntp[64];
  // pwm
  uint16_t pwmFreq;     // Mean Well 3-in-1: 100 Hz .. 3 kHz, keep <= 2 kHz for XLG
  uint8_t  pwmBits;
  // channels
  uint8_t numChannels;
  ChannelCfg ch[MAX_CHANNELS];
  // behaviour
  float master;         // 0..100 global scale
  float slewPctPerSec;  // max change rate of outputs (anti-flicker, soft start)
  float fallbackLevel;  // used while the clock is unknown
  // effects
  bool  lunarEnabled;  float lunarMax;              // max % at full moon
  bool  cloudsEnabled; float cloudDepth; uint16_t cloudMinGap; uint16_t cloudMaxGap; // gap in s
  bool  lightningEnabled;
  // acclimation (new plants / corals): start at acclStart %, reach 100 % after acclDays
  bool  acclEnabled; float acclStart; uint16_t acclDays; uint32_t acclStartEpoch;
  // thermal protection (needs DS18B20 or an externally posted temperature)
  float tempDimStart;   // begin dimming above this (C)
  float tempDimFull;    // reach tempDimFloor at this temperature
  float tempDimFloor;   // % of normal level at tempDimFull
  float tempShutdown;   // latch outputs off above this, until T < tempDimStart
  int8_t oneWirePin;
  // optional mains / DC relay: older HLG-xxxH-B drivers only dim to ~10 %, so a relay
  // gives a true "off" (and kills standby power on any driver). -1 = not used.
  int8_t   relayPin;
  bool     relayActiveLow;
  uint16_t relayOffDelayS; // keep the relay on this long after all channels reach 0
};

extern Config cfg;

void configDefaults();
bool configLoad();
bool configSave();
void configToJson(JsonObject o, bool includeSecrets);
void configFromJson(JsonObjectConst o);
