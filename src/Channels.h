#pragma once
#include <Arduino.h>
#include "Config.h"

// LEDC PWM outputs, one per channel. Levels are "logical" 0..100 %:
//  0            -> duty 0 (Mean Well dims to off at 0 % duty)
//  (0 .. 100]   -> mapped into [minPct .. maxPct] through the gamma curve
void  channelsBegin();                    // attach all enabled channels from cfg
void  channelsReconfigure();              // call after cfg changes (pins, freq, invert)
void  channelWrite(uint8_t i, float pct); // logical level
float channelDriverPct(uint8_t i);        // what the driver actually sees (0..100)
