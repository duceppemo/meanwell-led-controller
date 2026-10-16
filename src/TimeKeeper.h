#pragma once
#include <Arduino.h>
#include <time.h>

// NTP time with POSIX TZ. If NTP is unavailable after a reboot the last epoch
// persisted to NVS (every 5 min) is restored, flagged as "approximate", so the
// schedule keeps running at roughly the right point instead of a fallback level.
void   timeBegin();
void   timeLoop();
void   timeApplyConfig();        // after tz / ntp change
bool   timeSynced();             // NTP sync achieved since boot
bool   timeApprox();             // running on restored epoch
bool   timeKnown();              // synced || approx
float  timeMinuteOfDay();        // local time, fractional minutes
time_t timeNow();
String timeString();             // "YYYY-MM-DD HH:MM:SS"
void   timeSetManual(time_t epoch); // from browser if no NTP
