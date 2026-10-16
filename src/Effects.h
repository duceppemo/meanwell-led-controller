#pragma once
#include <Arduino.h>
#include <time.h>

// Lunar cycle: 0 = new moon, 0.5 = full moon. Illumination 0..1.
double moonPhase(time_t utc);
double moonIllumination(time_t utc);

// Passing clouds: a multiplicative factor (1 = clear sky) with random, smooth dips.
class CloudSim {
 public:
  void  setParams(float depthPct, uint32_t minGapS, uint32_t maxGapS);
  float factor(uint32_t nowMs);
  bool  active() const { return m_active; }
 private:
  float    m_depthPct = 30; uint32_t m_minGap = 60000, m_maxGap = 600000;
  bool     m_active = false; uint32_t m_next = 0, m_start = 0, m_dur = 0; float m_depth = 0;
};

// Lightning storm: short full-brightness pulses in random groups for `seconds`.
class LightningSim {
 public:
  void start(uint32_t seconds);
  void stop() { m_end = 0; }
  bool stormActive(uint32_t nowMs) const { return nowMs < m_end; }
  bool flashOn(uint32_t nowMs);
  uint32_t remainingS(uint32_t nowMs) const { return nowMs < m_end ? (m_end - nowMs) / 1000 : 0; }
 private:
  uint32_t m_end = 0, m_nextGroup = 0, m_pulseEnd = 0, m_gapEnd = 0;
  uint8_t  m_pulsesLeft = 0; bool m_inPulse = false;
};
