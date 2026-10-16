#include "Effects.h"
#include <math.h>

static const double SYNODIC = 29.530588853;

double moonPhase(time_t utc) {
  double jd = (double)utc / 86400.0 + 2440587.5;
  double phase = fmod((jd - 2451550.1) / SYNODIC, 1.0);   // 2451550.1 = new moon 2000-01-06
  if (phase < 0) phase += 1.0;
  return phase;
}
double moonIllumination(time_t utc) {
  return 0.5 - 0.5 * cos(2.0 * M_PI * moonPhase(utc));
}

void CloudSim::setParams(float depthPct, uint32_t minGapS, uint32_t maxGapS) {
  m_depthPct = depthPct; m_minGap = minGapS * 1000UL; m_maxGap = max(maxGapS, minGapS) * 1000UL;
  m_next = 0;
}

float CloudSim::factor(uint32_t now) {
  if (!m_active) {
    if (m_next == 0) m_next = now + random(m_minGap, m_maxGap + 1);
    if ((int32_t)(now - m_next) < 0) return 1.0f;
    m_active = true; m_start = now;
    m_dur = random(20000, 150000);
    m_depth = m_depthPct / 100.0f * (random(40, 101) / 100.0f);
  }
  float p = (float)(now - m_start) / (float)m_dur;
  if (p >= 1.0f) {
    m_active = false;
    m_next = now + random(m_minGap, m_maxGap + 1);
    return 1.0f;
  }
  return 1.0f - m_depth * (0.5f - 0.5f * cosf(2.0f * (float)PI * p));
}

void LightningSim::start(uint32_t seconds) {
  uint32_t now = millis();
  m_end = now + seconds * 1000UL;
  m_nextGroup = now + random(500, 3000);
  m_inPulse = false; m_pulsesLeft = 0;
}

bool LightningSim::flashOn(uint32_t now) {
  if (now >= m_end) return false;
  if (m_inPulse) {
    if ((int32_t)(now - m_pulseEnd) < 0) return true;
    m_inPulse = false; m_gapEnd = now + random(60, 220);
    return false;
  }
  if (m_pulsesLeft > 0) {
    if ((int32_t)(now - m_gapEnd) >= 0) {
      m_pulsesLeft--; m_inPulse = true; m_pulseEnd = now + random(40, 130);
      return true;
    }
    return false;
  }
  if ((int32_t)(now - m_nextGroup) >= 0) {
    m_pulsesLeft = random(1, 5); m_inPulse = true; m_pulseEnd = now + random(40, 130);
    m_nextGroup = now + random(2000, 15000);
    return true;
  }
  return false;
}
