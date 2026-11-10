#include "Knob.h"
#include "Config.h"
#include "Engine.h"
#include "soc/gpio_reg.h"
#include "soc/soc_caps.h"

static int8_t s_a = -1, s_b = -1, s_btn = -1;
static volatile int32_t s_transitions = 0;   // signed quadrature steps, written by the ISR
static volatile uint8_t s_state = 0;          // last A/B state (A << 1 | B)
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static int32_t  s_acc = 0;                    // steps not yet turned into detents
static uint32_t s_lastDetentAt = 0;
static bool     s_btnRaw = false, s_btnStable = false;
static uint32_t s_btnChangedAt = 0;

// Quadrature decoder: index = previous state << 2 | current state. Invalid jumps
// (contact bounce skipping a state) count as 0, and bounce back and forth cancels out.
static const int8_t QDEC[16] = { 0, -1, 1, 0,  1, 0, 0, -1,  -1, 0, 0, 1,  0, 1, -1, 0 };

// Register read instead of digitalRead(): safe from IRAM while flash is busy.
static inline int IRAM_ATTR fastRead(int pin) {
#if SOC_GPIO_PIN_COUNT > 32
  if (pin >= 32) return (REG_READ(GPIO_IN1_REG) >> (pin - 32)) & 1;
#endif
  return (REG_READ(GPIO_IN_REG) >> pin) & 1;
}

static void IRAM_ATTR onEdge() {
  uint8_t cur = (uint8_t)((fastRead(s_a) << 1) | fastRead(s_b));
  portENTER_CRITICAL_ISR(&s_mux);
  s_transitions += QDEC[(s_state << 2) | cur];
  s_state = cur;
  portEXIT_CRITICAL_ISR(&s_mux);
}

bool knobPresent() { return s_a >= 0 && s_b >= 0; }

void knobBegin() {
  if (cfg.knobA >= 0 && cfg.knobB >= 0 && cfg.knobA != cfg.knobB) {
    s_a = cfg.knobA; s_b = cfg.knobB;
    pinMode(s_a, INPUT_PULLUP);
    pinMode(s_b, INPUT_PULLUP);
    s_state = (uint8_t)((digitalRead(s_a) << 1) | digitalRead(s_b));
    attachInterrupt(digitalPinToInterrupt(s_a), onEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(s_b), onEdge, CHANGE);
    log_i("knob on GPIO %d / %d", s_a, s_b);
  }
  if (cfg.knobBtn >= 0 && cfg.knobBtn != PIN_BUTTON) {
    s_btn = cfg.knobBtn;
    pinMode(s_btn, INPUT_PULLUP);
  }
}

void knobLoop() {
  uint32_t now = millis();

  if (s_a >= 0) {
    portENTER_CRITICAL(&s_mux);
    int32_t t = s_transitions; s_transitions = 0;
    uint8_t state = s_state;
    portEXIT_CRITICAL(&s_mux);
    s_acc += t;
    int32_t div = cfg.knobDivider ? cfg.knobDivider : 4;
    int32_t detents = s_acc / div;            // truncates toward zero
    s_acc -= detents * div;
    // Full-step encoders rest at A=B=1 between clicks. Resync there so a click that
    // lost a step to contact bounce still counts once instead of skewing later clicks.
    if (div == 4 && state == 3 && s_acc != 0) {
      if (s_acc >= 2) detents++; else if (s_acc <= -2) detents--;
      s_acc = 0;
    }
    if (detents) {
      if (cfg.knobReverse) detents = -detents;
      // acceleration: fast spins move further per click
      uint32_t gap = now - s_lastDetentAt;
      float mult = gap < 40 ? 4.0f : gap < 100 ? 2.0f : 1.0f;
      s_lastDetentAt = now;
      engineKnobTurn((float)detents * cfg.knobStepPct * mult);
    }
  }

  if (s_btn >= 0) {
    bool raw = digitalRead(s_btn) == LOW;
    if (raw != s_btnRaw) { s_btnRaw = raw; s_btnChangedAt = now; }
    if (raw != s_btnStable && now - s_btnChangedAt > 30) {
      s_btnStable = raw;
      if (s_btnStable) engineKnobPress();      // act on press, not release
    }
  }
}
