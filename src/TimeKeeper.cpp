#include "TimeKeeper.h"
#include "Config.h"
#include <Preferences.h>
#include <esp_sntp.h>
#include <sys/time.h>

static bool s_synced = false, s_approx = false;
static uint32_t s_lastPersist = 0;
static Preferences s_prefs;

static void onSync(struct timeval*) { s_synced = true; s_approx = false; log_i("NTP time synced"); }

static void persist() {
  time_t n = time(nullptr);
  if (n > 1600000000) { s_prefs.putULong("epoch", (uint32_t)n); }
}

void timeApplyConfig() {
  setenv("TZ", cfg.tz, 1); tzset();
  configTzTime(cfg.tz, cfg.ntp, "time.google.com", "time.cloudflare.com");
}

void timeBegin() {
  s_prefs.begin("time", false);
  esp_sntp_set_time_sync_notification_cb(onSync);
  uint32_t stored = s_prefs.getULong("epoch", 0);
  if (stored > 1600000000) {
    struct timeval tv; tv.tv_sec = (time_t)stored + 150; tv.tv_usec = 0;  // ~ half the persist period
    settimeofday(&tv, nullptr);
    s_approx = true;
    log_w("time restored from NVS (approximate)");
  }
  timeApplyConfig();
}

void timeLoop() {
  if (millis() - s_lastPersist > 300000UL) { s_lastPersist = millis(); persist(); }
}

bool   timeSynced() { return s_synced; }
bool   timeApprox() { return s_approx && !s_synced; }
bool   timeKnown()  { return s_synced || s_approx; }
time_t timeNow()    { return time(nullptr); }

float timeMinuteOfDay() {
  struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
  return t.tm_hour * 60.0f + t.tm_min + t.tm_sec / 60.0f;
}

String timeString() {
  struct tm t; time_t n = time(nullptr); localtime_r(&n, &t);
  char b[24]; strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &t);
  return String(b);
}

void timeSetManual(time_t epoch) {
  struct timeval tv; tv.tv_sec = epoch; tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  if (!s_synced) s_approx = true;
  persist();
}
