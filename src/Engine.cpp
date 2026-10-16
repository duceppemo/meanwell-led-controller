#include "Engine.h"
#include "Config.h"
#include "Schedule.h"
#include "Channels.h"
#include "Effects.h"
#include "TimeKeeper.h"
#include "Sensors.h"

static Mode     s_mode = MODE_AUTO;
static uint32_t s_modeEnd = 0;           // ms, 0 = no timeout
static float    s_manual[MAX_CHANNELS];
static float    s_target[MAX_CHANNELS];
static float    s_output[MAX_CHANNELS];
static float    s_thermal = 1, s_accl = 1, s_cloud = 1;
static bool     s_shutdown = false, s_flash = false;
static double   s_moon = 0;
static uint32_t s_lastTick = 0;
static uint32_t s_previewStart = 0, s_previewLen = 0;
static float    s_simMinute = 0;
static CloudSim s_clouds;
static LightningSim s_lightning;
static bool     s_relayOn = false;
static int8_t   s_relayPin = -1;
static uint32_t s_relayOffAt = 0;

Mode engineMode() { return s_mode; }
bool engineRelayOn() { return s_relayOn; }

static void relayWrite(bool on) {
  s_relayOn = on;
  if (s_relayPin >= 0) digitalWrite(s_relayPin, (on != cfg.relayActiveLow) ? HIGH : LOW);
}

void engineConfigChanged() {
  s_clouds.setParams(cfg.cloudDepth, cfg.cloudMinGap, cfg.cloudMaxGap);
  if (cfg.relayPin != s_relayPin) {
    s_relayPin = cfg.relayPin;
    if (s_relayPin >= 0) { pinMode(s_relayPin, OUTPUT); relayWrite(s_relayOn); }
  }
}

static void relayLoop(uint32_t now, bool anyOn) {
  if (anyOn) { s_relayOffAt = 0; if (!s_relayOn) relayWrite(true); return; }
  if (s_relayOffAt == 0) s_relayOffAt = now + cfg.relayOffDelayS * 1000UL;
  if (s_relayOn && (int32_t)(now - s_relayOffAt) >= 0) relayWrite(false);
}

void engineBegin() {
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) { s_manual[i] = 0; s_target[i] = 0; s_output[i] = 0; }
  engineConfigChanged();
  s_lastTick = millis();
}

void engineSetMode(Mode m, uint32_t timeoutS) {
  s_mode = m;
  s_modeEnd = timeoutS ? millis() + timeoutS * 1000UL : 0;
}

void engineSetManual(const float* levels, uint8_t n) {
  for (uint8_t i = 0; i < MAX_CHANNELS && i < n; i++) s_manual[i] = constrain(levels[i], 0.0f, 100.0f);
  if (s_mode != MODE_MANUAL) engineSetMode(MODE_MANUAL, 0);
}

void engineStartPreview(uint16_t seconds) {
  s_previewStart = millis(); s_previewLen = max<uint32_t>(seconds, 10) * 1000UL;
  s_mode = MODE_PREVIEW; s_modeEnd = 0;
}

void engineTriggerStorm(uint16_t seconds) { s_lightning.start(seconds); }
void engineStopStorm() { s_lightning.stop(); }

static float thermalFactor() {
  float t = sensorTemperature();
  if (isnan(t)) { s_shutdown = false; return 1.0f; }
  if (t >= cfg.tempShutdown) s_shutdown = true;
  if (s_shutdown && t < cfg.tempDimStart) s_shutdown = false;
  if (s_shutdown) return 0.0f;
  if (t <= cfg.tempDimStart) return 1.0f;
  if (t >= cfg.tempDimFull)  return cfg.tempDimFloor / 100.0f;
  float f = (t - cfg.tempDimStart) / (cfg.tempDimFull - cfg.tempDimStart);
  return 1.0f - f * (1.0f - cfg.tempDimFloor / 100.0f);
}

static float acclFactor() {
  if (!cfg.acclEnabled || cfg.acclDays == 0) return 1.0f;
  time_t now = timeNow();
  float days = ((float)now - (float)cfg.acclStartEpoch) / 86400.0f;
  float p = constrain(days / (float)cfg.acclDays, 0.0f, 1.0f);
  return cfg.acclStart / 100.0f + (1.0f - cfg.acclStart / 100.0f) * p;
}

void engineLoop() {
  uint32_t now = millis();
  if (now - s_lastTick < 50) return;
  float dt = (now - s_lastTick) / 1000.0f;
  s_lastTick = now;

  if (s_modeEnd && (int32_t)(now - s_modeEnd) >= 0) { s_mode = MODE_AUTO; s_modeEnd = 0; }
  if (s_mode == MODE_PREVIEW && now - s_previewStart >= s_previewLen) s_mode = MODE_AUTO;

  s_thermal = thermalFactor();
  s_accl    = acclFactor();
  s_cloud   = cfg.cloudsEnabled ? s_clouds.factor(now) : 1.0f;
  s_flash   = cfg.lightningEnabled && s_lightning.flashOn(now);
  s_moon    = moonIllumination(timeNow());

  const Profile& pr = profiles.p[profiles.active];
  float minute;
  bool clockOk = timeKnown();
  if (s_mode == MODE_PREVIEW) {
    s_simMinute = 1440.0f * (float)(now - s_previewStart) / (float)s_previewLen;
    minute = s_simMinute;
  } else {
    minute = timeMinuteOfDay();
  }

  bool anyOn = false;
  for (uint8_t i = 0; i < cfg.numChannels; i++) {
    const ChannelCfg& c = cfg.ch[i];
    float lvl = 0;
    switch (s_mode) {
      case MODE_OFF:    lvl = 0; break;
      case MODE_MANUAL: lvl = s_manual[i]; break;
      case MODE_PREVIEW:
      case MODE_AUTO:
        lvl = (clockOk || s_mode == MODE_PREVIEW) ? scheduleLevelAt(pr.ch[i], minute, pr.smooth) : cfg.fallbackLevel;
        if (c.effects) lvl *= s_cloud;
        if (cfg.lunarEnabled && c.moon) {
          float moonLvl = cfg.lunarMax * (float)s_moon;
          if (moonLvl > lvl) lvl = moonLvl;
        }
        break;
    }
    lvl *= (cfg.master / 100.0f) * s_accl;
    if (s_mode != MODE_OFF) lvl *= s_thermal;
    if (s_flash && c.effects && s_mode != MODE_OFF && !s_shutdown) lvl = 100.0f;
    s_target[i] = constrain(lvl, 0.0f, 100.0f);

    // slew limit (bypassed for lightning, preview and thermal shutdown)
    float step = cfg.slewPctPerSec * dt;
    bool instant = s_flash || s_mode == MODE_PREVIEW || s_shutdown;
    if (instant || fabsf(s_target[i] - s_output[i]) <= step) s_output[i] = s_target[i];
    else s_output[i] += (s_target[i] > s_output[i]) ? step : -step;
    channelWrite(i, s_output[i]);
    if (c.enabled && (s_output[i] > 0.0f || s_target[i] > 0.0f)) anyOn = true;
  }
  relayLoop(now, anyOn);
}

void engineStatusJson(JsonObject o) {
  static const char* names[] = { "auto", "manual", "off", "preview" };
  o["mode"] = names[s_mode];
  o["modeTimeoutS"] = s_modeEnd ? (int32_t)(s_modeEnd - millis()) / 1000 : 0;
  o["profile"] = profiles.p[profiles.active].name;
  o["profileIndex"] = profiles.active;
  o["thermal"] = s_thermal; o["acclimation"] = s_accl; o["cloud"] = s_cloud;
  o["shutdown"] = s_shutdown; o["flash"] = s_flash; o["relay"] = s_relayOn;
  o["stormS"] = s_lightning.remainingS(millis());
  o["moon"] = s_moon;
  o["moonPhase"] = moonPhase(timeNow());
  if (s_mode == MODE_PREVIEW) o["simMinute"] = s_simMinute;
  JsonArray ch = o["channels"].to<JsonArray>();
  for (uint8_t i = 0; i < cfg.numChannels; i++) {
    JsonObject j = ch.add<JsonObject>();
    j["name"] = cfg.ch[i].name; j["enabled"] = cfg.ch[i].enabled;
    j["target"] = s_target[i]; j["output"] = s_output[i]; j["driver"] = channelDriverPct(i);
    j["manual"] = s_manual[i];
  }
}
