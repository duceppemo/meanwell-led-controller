#include "Sensors.h"
#include "Config.h"
#include <OneWire.h>
#include <DallasTemperature.h>

static OneWire* s_ow = nullptr;
static DallasTemperature* s_ds = nullptr;
static bool s_present = false, s_pending = false;
static float s_local = NAN, s_extT = NAN, s_extH = NAN;
static uint32_t s_lastReq = 0, s_extAt = 0;
static const uint32_t EXT_STALE_MS = 600000;

void sensorsBegin() {
  if (cfg.oneWirePin < 0) return;
  s_ow = new OneWire(cfg.oneWirePin);
  s_ds = new DallasTemperature(s_ow);
  s_ds->begin();
  s_ds->setWaitForConversion(false);
  s_ds->setResolution(11);
  s_present = s_ds->getDS18Count() > 0;
  log_i("DS18B20 sensors: %u", s_ds->getDS18Count());
}

void sensorsLoop() {
  if (!s_ds) return;
  uint32_t now = millis();
  if (!s_pending && now - s_lastReq > 10000) {
    s_ds->requestTemperatures(); s_pending = true; s_lastReq = now;
  } else if (s_pending && now - s_lastReq > 800) {
    s_pending = false;
    float t = s_ds->getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C || t < -50 || t > 125) { s_local = NAN; s_present = false; }
    else { s_local = t; s_present = true; }
  }
}

bool  sensorLocalPresent() { return s_present; }
float sensorTemperature() {
  if (!isnan(s_local)) return s_local;
  if (!isnan(s_extT) && millis() - s_extAt < EXT_STALE_MS) return s_extT;
  return NAN;
}
float sensorHumidity() {
  if (!isnan(s_extH) && millis() - s_extAt < EXT_STALE_MS) return s_extH;
  return NAN;
}
void sensorsSetExternal(float t, float h) { s_extT = t; s_extH = h; s_extAt = millis(); }
