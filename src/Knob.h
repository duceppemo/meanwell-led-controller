#pragma once
#include <Arduino.h>

// Optional rotary encoder with push button (EC11 / KY-040 style) for dimming at the
// fixture without the web UI. Turn: switch to Manual starting from the current level
// and step it. Press: return to Auto. Pins come from cfg.knobA / knobB / knobBtn
// (-1 = not fitted) and are applied at boot.
void knobBegin();
void knobLoop();
bool knobPresent();
