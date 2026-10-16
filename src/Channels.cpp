#include "Channels.h"

static float    s_driverPct[MAX_CHANNELS];
static int8_t   s_attachedPin[MAX_CHANNELS];
static uint32_t s_maxDuty = 4095;

static void detachAll() {
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) {
    if (s_attachedPin[i] >= 0) { ledcDetach(s_attachedPin[i]); s_attachedPin[i] = -1; }
  }
}

void channelsReconfigure() {
  detachAll();
  s_maxDuty = (1UL << cfg.pwmBits) - 1;
  for (uint8_t i = 0; i < cfg.numChannels; i++) {
    const ChannelCfg& c = cfg.ch[i];
    if (!c.enabled || c.pin < 0) continue;
    if (!ledcAttach(c.pin, cfg.pwmFreq, cfg.pwmBits)) {
      log_e("ledcAttach failed ch%u pin %d", i, c.pin);
      continue;
    }
    ledcOutputInvert(c.pin, c.invert);
    s_attachedPin[i] = c.pin;
    ledcWrite(c.pin, 0);
    s_driverPct[i] = 0;
  }
}

void channelsBegin() {
  for (uint8_t i = 0; i < MAX_CHANNELS; i++) { s_attachedPin[i] = -1; s_driverPct[i] = 0; }
  channelsReconfigure();
}

void channelWrite(uint8_t i, float pct) {
  if (i >= cfg.numChannels || s_attachedPin[i] < 0) return;
  const ChannelCfg& c = cfg.ch[i];
  float out;
  if (pct <= 0.0f) {
    out = 0.0f;
  } else {
    float x = constrain(pct, 0.0f, 100.0f) / 100.0f;
    if (c.gamma != 1.0f) x = powf(x, c.gamma);
    out = c.minPct + (c.maxPct - c.minPct) * x;
    out = constrain(out, 0.0f, 100.0f);
  }
  s_driverPct[i] = out;
  ledcWrite(c.pin, (uint32_t)lroundf(out / 100.0f * s_maxDuty));
}

float channelDriverPct(uint8_t i) { return i < MAX_CHANNELS ? s_driverPct[i] : 0; }
