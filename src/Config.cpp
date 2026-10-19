#include "Config.h"
#include <LittleFS.h>

Config cfg;
static const char* CFG_PATH = "/config.json";

static void setStr(char* dst, size_t n, JsonVariantConst v) {
  if (v.is<const char*>()) strlcpy(dst, v.as<const char*>(), n);
}

void configDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  strlcpy(cfg.devName, "LED Controller", sizeof(cfg.devName));
  strlcpy(cfg.hostname, "ledctrl", sizeof(cfg.hostname));
  strlcpy(cfg.tz, "EST5EDT,M3.2.0,M11.1.0", sizeof(cfg.tz));
  strlcpy(cfg.ntp, "pool.ntp.org", sizeof(cfg.ntp));
  cfg.pwmFreq = 1000;
  cfg.pwmBits = 12;
  const int8_t pins[] = DEFAULT_CH_PINS;
  cfg.numChannels = DEFAULT_NUM_CHANNELS;
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    ChannelCfg& c = cfg.ch[i];
    snprintf(c.name, sizeof(c.name), "Channel %u", i + 1);
    c.pin = pins[i];
    c.enabled = i < cfg.numChannels;
    c.invert = false;
    c.minPct = 8;
    c.maxPct = 100;
    c.gamma = 1.0f;
    c.moon = false;
    c.effects = true;
  }
  cfg.master = 100;
  cfg.slewPctPerSec = 5;
  cfg.fallbackLevel = 50;
  cfg.lunarEnabled = false;  cfg.lunarMax = 5;
  cfg.cloudsEnabled = false; cfg.cloudDepth = 30; cfg.cloudMinGap = 60; cfg.cloudMaxGap = 600;
  cfg.lightningEnabled = false;
  cfg.acclEnabled = false; cfg.acclStart = 50; cfg.acclDays = 14; cfg.acclStartEpoch = 0;
  cfg.tempDimStart = 45; cfg.tempDimFull = 55; cfg.tempDimFloor = 30; cfg.tempShutdown = 65;
  cfg.oneWirePin = PIN_ONEWIRE;
  cfg.relayPin = -1; cfg.relayActiveLow = false; cfg.relayOffDelayS = 60;
}

void configToJson(JsonObject o, bool includeSecrets) {
  JsonObject d = o["device"].to<JsonObject>();
  d["name"] = cfg.devName; d["hostname"] = cfg.hostname;
  JsonObject w = o["wifi"].to<JsonObject>();
  w["ssid"] = cfg.ssid;
  w["pass"] = includeSecrets ? cfg.pass : (cfg.pass[0] ? "********" : "");
  w["apPass"] = includeSecrets ? cfg.apPass : (cfg.apPass[0] ? "********" : "");
  JsonObject t = o["time"].to<JsonObject>();
  t["tz"] = cfg.tz; t["ntp"] = cfg.ntp;
  JsonObject p = o["pwm"].to<JsonObject>();
  p["freq"] = cfg.pwmFreq; p["bits"] = cfg.pwmBits;
  o["numChannels"] = cfg.numChannels;
  JsonArray chs = o["channels"].to<JsonArray>();
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    const ChannelCfg& c = cfg.ch[i];
    JsonObject j = chs.add<JsonObject>();
    j["name"] = c.name; j["pin"] = c.pin; j["enabled"] = c.enabled; j["invert"] = c.invert;
    j["min"] = c.minPct; j["max"] = c.maxPct; j["gamma"] = c.gamma;
    j["moon"] = c.moon; j["effects"] = c.effects;
  }
  o["master"] = cfg.master;
  o["slew"] = cfg.slewPctPerSec;
  o["fallbackLevel"] = cfg.fallbackLevel;
  JsonObject e = o["effects"].to<JsonObject>();
  JsonObject l = e["lunar"].to<JsonObject>();   l["enabled"] = cfg.lunarEnabled; l["max"] = cfg.lunarMax;
  JsonObject c = e["clouds"].to<JsonObject>();  c["enabled"] = cfg.cloudsEnabled; c["depth"] = cfg.cloudDepth;
  c["minGap"] = cfg.cloudMinGap; c["maxGap"] = cfg.cloudMaxGap;
  JsonObject li = e["lightning"].to<JsonObject>(); li["enabled"] = cfg.lightningEnabled;
  JsonObject a = o["acclimation"].to<JsonObject>();
  a["enabled"] = cfg.acclEnabled; a["start"] = cfg.acclStart; a["days"] = cfg.acclDays; a["startEpoch"] = cfg.acclStartEpoch;
  JsonObject pr = o["protection"].to<JsonObject>();
  pr["dimStart"] = cfg.tempDimStart; pr["dimFull"] = cfg.tempDimFull; pr["dimFloor"] = cfg.tempDimFloor; pr["shutdown"] = cfg.tempShutdown;
  JsonObject s = o["sensors"].to<JsonObject>();
  s["oneWirePin"] = cfg.oneWirePin;
  JsonObject r = o["relay"].to<JsonObject>();
  r["pin"] = cfg.relayPin; r["activeLow"] = cfg.relayActiveLow; r["offDelay"] = cfg.relayOffDelayS;
}

void configFromJson(JsonObjectConst o) {
  JsonObjectConst d = o["device"];
  setStr(cfg.devName, sizeof(cfg.devName), d["name"]);
  setStr(cfg.hostname, sizeof(cfg.hostname), d["hostname"]);
  JsonObjectConst w = o["wifi"];
  setStr(cfg.ssid, sizeof(cfg.ssid), w["ssid"]);
  if (w["pass"].is<const char*>() && strcmp(w["pass"], "********") != 0) setStr(cfg.pass, sizeof(cfg.pass), w["pass"]);
  if (w["apPass"].is<const char*>() && strcmp(w["apPass"], "********") != 0) setStr(cfg.apPass, sizeof(cfg.apPass), w["apPass"]);
  JsonObjectConst t = o["time"];
  setStr(cfg.tz, sizeof(cfg.tz), t["tz"]);
  setStr(cfg.ntp, sizeof(cfg.ntp), t["ntp"]);
  JsonObjectConst p = o["pwm"];
  cfg.pwmFreq = constrain((int)(p["freq"] | (int)cfg.pwmFreq), 100, 3000);
  cfg.pwmBits = constrain((int)(p["bits"] | (int)cfg.pwmBits), 8, 14);
  cfg.numChannels = constrain((int)(o["numChannels"] | (int)cfg.numChannels), 1, MAX_CHANNELS);
  JsonArrayConst chs = o["channels"];
  uint8_t i = 0;
  for (JsonObjectConst j : chs) {
    if (i >= MAX_CHANNELS) break;
    ChannelCfg& c = cfg.ch[i];
    setStr(c.name, sizeof(c.name), j["name"]);
    c.pin = j["pin"] | c.pin;
    c.enabled = j["enabled"] | c.enabled;
    c.invert = j["invert"] | c.invert;
    c.minPct = constrain((float)(j["min"] | c.minPct), 0.0f, 100.0f);
    c.maxPct = constrain((float)(j["max"] | c.maxPct), 0.0f, 100.0f);
    c.gamma = constrain((float)(j["gamma"] | c.gamma), 0.2f, 4.0f);
    c.moon = j["moon"] | c.moon;
    c.effects = j["effects"] | c.effects;
    i++;
  }
  cfg.master = constrain((float)(o["master"] | cfg.master), 0.0f, 100.0f);
  cfg.slewPctPerSec = constrain((float)(o["slew"] | cfg.slewPctPerSec), 0.1f, 1000.0f);
  cfg.fallbackLevel = constrain((float)(o["fallbackLevel"] | cfg.fallbackLevel), 0.0f, 100.0f);
  JsonObjectConst e = o["effects"];
  cfg.lunarEnabled = e["lunar"]["enabled"] | cfg.lunarEnabled;
  cfg.lunarMax = constrain((float)(e["lunar"]["max"] | cfg.lunarMax), 0.0f, 100.0f);
  cfg.cloudsEnabled = e["clouds"]["enabled"] | cfg.cloudsEnabled;
  cfg.cloudDepth = constrain((float)(e["clouds"]["depth"] | cfg.cloudDepth), 0.0f, 100.0f);
  cfg.cloudMinGap = constrain((int)(e["clouds"]["minGap"] | (int)cfg.cloudMinGap), 5, 7200);
  cfg.cloudMaxGap = constrain((int)(e["clouds"]["maxGap"] | (int)cfg.cloudMaxGap), (int)cfg.cloudMinGap, 7200);
  cfg.lightningEnabled = e["lightning"]["enabled"] | cfg.lightningEnabled;
  JsonObjectConst a = o["acclimation"];
  cfg.acclEnabled = a["enabled"] | cfg.acclEnabled;
  cfg.acclStart = constrain((float)(a["start"] | cfg.acclStart), 0.0f, 100.0f);
  cfg.acclDays = constrain((int)(a["days"] | (int)cfg.acclDays), 1, 365);
  cfg.acclStartEpoch = a["startEpoch"] | cfg.acclStartEpoch;
  JsonObjectConst pr = o["protection"];
  cfg.tempDimStart = pr["dimStart"] | cfg.tempDimStart;
  cfg.tempDimFull = pr["dimFull"] | cfg.tempDimFull;
  cfg.tempDimFloor = constrain((float)(pr["dimFloor"] | cfg.tempDimFloor), 0.0f, 100.0f);
  cfg.tempShutdown = pr["shutdown"] | cfg.tempShutdown;
  cfg.oneWirePin = o["sensors"]["oneWirePin"] | cfg.oneWirePin;
  JsonObjectConst r = o["relay"];
  cfg.relayPin = r["pin"] | cfg.relayPin;
  cfg.relayActiveLow = r["activeLow"] | cfg.relayActiveLow;
  cfg.relayOffDelayS = constrain((int)(r["offDelay"] | (int)cfg.relayOffDelayS), 0, 3600);
}

bool configLoad() {
  configDefaults();
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) { log_w("no config, using defaults"); return false; }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { log_e("config parse error: %s", err.c_str()); return false; }
  configFromJson(doc.as<JsonObjectConst>());
  return true;
}

bool configSave() {
  JsonDocument doc;
  configToJson(doc.to<JsonObject>(), true);
  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) { log_e("cannot write config"); return false; }
  serializeJson(doc, f);
  f.close();
  return true;
}
