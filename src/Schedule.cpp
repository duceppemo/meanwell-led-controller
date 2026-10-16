#include "Schedule.h"
#include <LittleFS.h>
#include <math.h>

Profiles profiles;
static const char* PATH = "/profiles.json";

static void sortPoints(ChannelSchedule& s) {
  for (uint8_t i = 1; i < s.n; i++) {
    SchedPoint k = s.pts[i]; int j = i - 1;
    while (j >= 0 && s.pts[j].minute > k.minute) { s.pts[j + 1] = s.pts[j]; j--; }
    s.pts[j + 1] = k;
  }
}

static void setRamp(ChannelSchedule& s, uint16_t riseStart, uint16_t riseEnd, uint16_t setStart, uint16_t setEnd, float peak) {
  s.n = 4;
  s.pts[0] = { riseStart, 0 };
  s.pts[1] = { riseEnd, peak };
  s.pts[2] = { setStart, peak };
  s.pts[3] = { setEnd, 0 };
}

void profilesDefaults() {
  memset(&profiles, 0, sizeof(profiles));
  profiles.count = 3; profiles.active = 0;
  const char* names[3] = { "Veg 18/6", "Bloom 12/12", "Seedling" };
  for (uint8_t p = 0; p < 3; p++) {
    Profile& pr = profiles.p[p];
    strlcpy(pr.name, names[p], sizeof(pr.name));
    pr.smooth = true;
    for (uint8_t c = 0; c < MAX_CHANNELS; c++) {
      if (p == 0)      setRamp(pr.ch[c], 6 * 60, 6 * 60 + 45, 23 * 60 + 15, 24 * 60 - 1, 100);  // 06:00-24:00
      else if (p == 1) setRamp(pr.ch[c], 8 * 60, 8 * 60 + 30, 19 * 60 + 30, 20 * 60, 100);        // 08:00-20:00
      else             setRamp(pr.ch[c], 6 * 60, 7 * 60, 23 * 60, 24 * 60 - 1, 40);                // gentle
    }
  }
}

// Level at minute t (0..1440). Points are sorted; the segment from the last
// point back to the first one wraps through midnight.
float scheduleLevelAt(const ChannelSchedule& s, float t, bool smooth) {
  if (s.n == 0) return 0;
  if (s.n == 1) return s.pts[0].level;
  t = fmodf(t, 1440.0f); if (t < 0) t += 1440.0f;
  const SchedPoint* a = nullptr; const SchedPoint* b = nullptr;
  float ta = 0, tb = 0;
  for (uint8_t i = 0; i + 1 < s.n; i++) {
    if (t >= s.pts[i].minute && t < s.pts[i + 1].minute) {
      a = &s.pts[i]; b = &s.pts[i + 1]; ta = a->minute; tb = b->minute; break;
    }
  }
  if (!a) {  // wrap segment: last point -> first point of the next day
    a = &s.pts[s.n - 1]; b = &s.pts[0];
    if (t >= a->minute) { ta = a->minute; tb = b->minute + 1440.0f; }
    else                { ta = a->minute - 1440.0f; tb = b->minute; }
  }
  float span = tb - ta;
  if (span <= 0) return b->level;
  float f = constrain((t - ta) / span, 0.0f, 1.0f);
  if (smooth) f = 0.5f - 0.5f * cosf(f * (float)PI);
  return a->level + (b->level - a->level) * f;
}

void profilesToJson(JsonObject o) {
  o["active"] = profiles.active;
  JsonArray arr = o["profiles"].to<JsonArray>();
  for (uint8_t p = 0; p < profiles.count; p++) {
    const Profile& pr = profiles.p[p];
    JsonObject j = arr.add<JsonObject>();
    j["name"] = pr.name; j["smooth"] = pr.smooth;
    JsonArray chs = j["channels"].to<JsonArray>();
    for (uint8_t c = 0; c < MAX_CHANNELS; c++) {
      JsonArray pts = chs.add<JsonArray>();
      for (uint8_t i = 0; i < pr.ch[c].n; i++) {
        JsonArray pt = pts.add<JsonArray>();
        pt.add(pr.ch[c].pts[i].minute); pt.add(pr.ch[c].pts[i].level);
      }
    }
  }
}

void profilesFromJson(JsonObjectConst o) {
  JsonArrayConst arr = o["profiles"];
  if (!arr.isNull()) {
    uint8_t p = 0;
    for (JsonObjectConst j : arr) {
      if (p >= MAX_PROFILES) break;
      Profile& pr = profiles.p[p];
      memset(&pr, 0, sizeof(pr));
      strlcpy(pr.name, j["name"] | "Profile", sizeof(pr.name));
      pr.smooth = j["smooth"] | true;
      uint8_t c = 0;
      for (JsonArrayConst pts : j["channels"].as<JsonArrayConst>()) {
        if (c >= MAX_CHANNELS) break;
        ChannelSchedule& s = pr.ch[c];
        s.n = 0;
        for (JsonArrayConst pt : pts) {
          if (s.n >= MAX_POINTS) break;
          s.pts[s.n].minute = constrain((int)(pt[0] | 0), 0, 1439);
          s.pts[s.n].level = constrain((float)(pt[1] | 0.0f), 0.0f, 100.0f);
          s.n++;
        }
        sortPoints(s);
        c++;
      }
      p++;
    }
    profiles.count = p;
  }
  profiles.active = constrain((int)(o["active"] | (int)profiles.active), 0, max(0, profiles.count - 1));
}

bool profilesLoad() {
  profilesDefaults();
  File f = LittleFS.open(PATH, "r");
  if (!f) return false;
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) { log_e("profiles parse error: %s", err.c_str()); profilesDefaults(); return false; }
  profilesFromJson(doc.as<JsonObjectConst>());
  if (profiles.count == 0) profilesDefaults();
  return true;
}

bool profilesSave() {
  JsonDocument doc;
  profilesToJson(doc.to<JsonObject>());
  File f = LittleFS.open(PATH, "w");
  if (!f) return false;
  serializeJson(doc, f);
  f.close();
  return true;
}
