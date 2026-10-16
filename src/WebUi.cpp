#include "WebUi.h"
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
#include "Config.h"
#include "Schedule.h"
#include "Channels.h"
#include "Engine.h"
#include "TimeKeeper.h"
#include "WifiManager.h"
#include "Sensors.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

static AsyncWebServer server(80);

// deferred actions (set from the async task, executed in webLoop)
enum : uint32_t {
  ACT_APPLY_CONFIG = 1 << 0, ACT_SAVE_CONFIG = 1 << 1, ACT_SAVE_PROFILES = 1 << 2,
  ACT_WIFI_CONNECT = 1 << 3, ACT_WIFI_AP = 1 << 4, ACT_REBOOT = 1 << 5,
  ACT_FACTORY = 1 << 6, ACT_TIME_APPLY = 1 << 7,
};
static volatile uint32_t s_actions = 0;
static uint32_t s_actionAt = 0;
static char s_newSsid[33], s_newPass[65];
static bool s_updateOk = false;
static String s_updateErr;

static void queue(uint32_t a) { s_actions |= a; s_actionAt = millis(); }

static void sendJson(AsyncWebServerRequest* req, JsonDocument& doc, int code = 200) {
  AsyncResponseStream* res = req->beginResponseStream("application/json");
  res->setCode(code);
  serializeJson(doc, *res);
  req->send(res);
}
static void sendOk(AsyncWebServerRequest* req) { req->send(200, "application/json", "{\"ok\":true}"); }
static void sendErr(AsyncWebServerRequest* req, int code, const char* msg) {
  JsonDocument d; d["ok"] = false; d["error"] = msg; sendJson(req, d, code);
}

static void statusJson(JsonObject o) {
  o["fw"] = FW_VERSION;
  o["name"] = cfg.devName;
  o["uptimeS"] = millis() / 1000;
  o["heap"] = ESP.getFreeHeap();
  JsonObject t = o["time"].to<JsonObject>();
  t["now"] = timeString(); t["epoch"] = (uint32_t)timeNow(); t["synced"] = timeSynced();
  t["approx"] = timeApprox(); t["known"] = timeKnown(); t["minute"] = timeMinuteOfDay(); t["tz"] = cfg.tz;
  wifiStatusJson(o["wifi"].to<JsonObject>());
  JsonObject s = o["sensors"].to<JsonObject>();
  float tc = sensorTemperature(), h = sensorHumidity();
  if (!isnan(tc)) s["temp"] = tc; else s["temp"] = nullptr;
  if (!isnan(h)) s["hum"] = h; else s["hum"] = nullptr;
  s["local"] = sensorLocalPresent();
  engineStatusJson(o["engine"].to<JsonObject>());
  o["master"] = cfg.master;
}

static bool isCaptiveHost(AsyncWebServerRequest* req) {
  if (!wifiIsAP()) return false;
  String host = req->host();
  String apIp = WiFi.softAPIP().toString();
  return host != apIp && host != String(cfg.hostname) && host != String(cfg.hostname) + ".local";
}

// ---------- OTA ----------
static void onUpdateBody(AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
  if (index == 0) {
    bool fs = (req->hasParam("type") && req->getParam("type")->value() == "fs") || filename.indexOf("littlefs") >= 0;
    s_updateOk = false; s_updateErr = "";
    log_i("OTA start: %s (%s)", filename.c_str(), fs ? "filesystem" : "firmware");
    if (fs) LittleFS.end();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, fs ? U_SPIFFS : U_FLASH)) { s_updateErr = Update.errorString(); return; }
  }
  if (s_updateErr.length()) return;
  if (len && Update.write(data, len) != len) { s_updateErr = Update.errorString(); Update.abort(); return; }
  if (final) {
    if (Update.end(true)) { s_updateOk = true; log_i("OTA done (%u bytes)", (unsigned)(index + len)); }
    else s_updateErr = Update.errorString();
  }
}

void webBegin() {
  // ---- status / live data
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d; statusJson(d.to<JsonObject>()); sendJson(r, d);
  });

  // ---- config
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d; configToJson(d.to<JsonObject>(), false);
    d["maxChannels"] = MAX_CHANNELS; d["maxProfiles"] = MAX_PROFILES; d["maxPoints"] = MAX_POINTS;
    sendJson(r, d);
  });
  auto* hCfg = new AsyncCallbackJsonWebHandler("/api/config", [](AsyncWebServerRequest* r, JsonVariant& json) {
    if (!json.is<JsonObject>()) return sendErr(r, 400, "object expected");
    configFromJson(json.as<JsonObjectConst>());
    queue(ACT_APPLY_CONFIG | ACT_SAVE_CONFIG | ACT_TIME_APPLY);
    sendOk(r);
  });
  hCfg->setMethod(HTTP_POST | HTTP_PUT);
  hCfg->setMaxContentLength(16384);
  server.addHandler(hCfg);

  // ---- profiles / schedules
  server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d; profilesToJson(d.to<JsonObject>()); sendJson(r, d);
  });
  auto* hProf = new AsyncCallbackJsonWebHandler("/api/profiles", [](AsyncWebServerRequest* r, JsonVariant& json) {
    if (!json.is<JsonObject>()) return sendErr(r, 400, "object expected");
    profilesFromJson(json.as<JsonObjectConst>());
    if (profiles.count == 0) profilesDefaults();
    queue(ACT_SAVE_PROFILES);
    sendOk(r);
  });
  hProf->setMethod(HTTP_POST | HTTP_PUT);
  hProf->setMaxContentLength(49152);
  server.addHandler(hProf);

  auto* hActive = new AsyncCallbackJsonWebHandler("/api/profiles/active", [](AsyncWebServerRequest* r, JsonVariant& json) {
    int idx = json["index"] | -1;
    if (idx < 0 || idx >= profiles.count) return sendErr(r, 400, "bad index");
    profiles.active = idx;
    queue(ACT_SAVE_PROFILES);
    sendOk(r);
  });
  hActive->setMethod(HTTP_POST);
  server.addHandler(hActive);

  // ---- control
  auto* hMode = new AsyncCallbackJsonWebHandler("/api/mode", [](AsyncWebServerRequest* r, JsonVariant& json) {
    const char* m = json["mode"] | "auto";
    uint32_t timeout = json["timeoutS"] | 0;
    if (!strcmp(m, "auto")) engineSetMode(MODE_AUTO);
    else if (!strcmp(m, "off")) engineSetMode(MODE_OFF, timeout);
    else if (!strcmp(m, "manual")) {
      JsonArrayConst lv = json["levels"];
      if (!lv.isNull()) {
        float levels[MAX_CHANNELS]; uint8_t n = 0;
        for (JsonVariantConst v : lv) { if (n >= MAX_CHANNELS) break; levels[n++] = v.as<float>(); }
        engineSetManual(levels, n);
      }
      engineSetMode(MODE_MANUAL, timeout);
    } else return sendErr(r, 400, "mode must be auto|manual|off");
    sendOk(r);
  });
  hMode->setMethod(HTTP_POST);
  server.addHandler(hMode);

  auto* hMaster = new AsyncCallbackJsonWebHandler("/api/master", [](AsyncWebServerRequest* r, JsonVariant& json) {
    cfg.master = constrain((float)(json["master"] | cfg.master), 0.0f, 100.0f);
    queue(ACT_SAVE_CONFIG);
    sendOk(r);
  });
  hMaster->setMethod(HTTP_POST);
  server.addHandler(hMaster);

  auto* hPreview = new AsyncCallbackJsonWebHandler("/api/preview", [](AsyncWebServerRequest* r, JsonVariant& json) {
    bool stop = json["stop"] | false;
    if (stop) engineSetMode(MODE_AUTO); else engineStartPreview(json["seconds"] | 60);
    sendOk(r);
  });
  hPreview->setMethod(HTTP_POST);
  server.addHandler(hPreview);

  auto* hStorm = new AsyncCallbackJsonWebHandler("/api/storm", [](AsyncWebServerRequest* r, JsonVariant& json) {
    bool stop = json["stop"] | false;
    if (stop) engineStopStorm(); else engineTriggerStorm(json["seconds"] | 60);
    sendOk(r);
  });
  hStorm->setMethod(HTTP_POST);
  server.addHandler(hStorm);

  auto* hAccl = new AsyncCallbackJsonWebHandler("/api/acclimation/start", [](AsyncWebServerRequest* r, JsonVariant& json) {
    cfg.acclEnabled = true;
    cfg.acclStartEpoch = (uint32_t)timeNow();
    if (json["start"].is<float>()) cfg.acclStart = constrain(json["start"].as<float>(), 0.0f, 100.0f);
    if (json["days"].is<int>()) cfg.acclDays = constrain(json["days"].as<int>(), 1, 365);
    queue(ACT_SAVE_CONFIG);
    sendOk(r);
  });
  hAccl->setMethod(HTTP_POST);
  server.addHandler(hAccl);

  // ---- sensors posted by an external system (Home Assistant etc.)
  auto* hSens = new AsyncCallbackJsonWebHandler("/api/sensors", [](AsyncWebServerRequest* r, JsonVariant& json) {
    float t = json["temp"] | NAN, h = json["hum"] | NAN;
    sensorsSetExternal(t, h);
    sendOk(r);
  });
  hSens->setMethod(HTTP_POST);
  server.addHandler(hSens);

  // ---- time
  auto* hTime = new AsyncCallbackJsonWebHandler("/api/time", [](AsyncWebServerRequest* r, JsonVariant& json) {
    uint32_t epoch = json["epoch"] | 0;
    if (epoch < 1600000000UL) return sendErr(r, 400, "bad epoch");
    timeSetManual((time_t)epoch);
    sendOk(r);
  });
  hTime->setMethod(HTTP_POST);
  server.addHandler(hTime);

  // ---- wifi
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d; wifiScanJson(d["networks"].to<JsonArray>()); sendJson(r, d);
  });
  auto* hWifi = new AsyncCallbackJsonWebHandler("/api/wifi", [](AsyncWebServerRequest* r, JsonVariant& json) {
    const char* ssid = json["ssid"] | "";
    if (!ssid[0]) return sendErr(r, 400, "ssid required");
    strlcpy(s_newSsid, ssid, sizeof(s_newSsid));
    strlcpy(s_newPass, json["pass"] | "", sizeof(s_newPass));
    queue(ACT_WIFI_CONNECT);
    sendOk(r);
  });
  hWifi->setMethod(HTTP_POST);
  server.addHandler(hWifi);
  server.on("/api/wifi/ap", HTTP_POST, [](AsyncWebServerRequest* r) { queue(ACT_WIFI_AP); sendOk(r); });

  // ---- backup / restore
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument d;
    d["fw"] = FW_VERSION;
    configToJson(d["config"].to<JsonObject>(), true);
    profilesToJson(d["profiles"].to<JsonObject>());
    AsyncResponseStream* res = r->beginResponseStream("application/json");
    res->addHeader("Content-Disposition", "attachment; filename=\"ledctrl-backup.json\"");
    serializeJsonPretty(d, *res);
    r->send(res);
  });
  auto* hRestore = new AsyncCallbackJsonWebHandler("/api/restore", [](AsyncWebServerRequest* r, JsonVariant& json) {
    if (json["config"].is<JsonObjectConst>()) { configFromJson(json["config"].as<JsonObjectConst>()); queue(ACT_APPLY_CONFIG | ACT_SAVE_CONFIG | ACT_TIME_APPLY); }
    if (json["profiles"].is<JsonObjectConst>()) { profilesFromJson(json["profiles"].as<JsonObjectConst>()); queue(ACT_SAVE_PROFILES); }
    sendOk(r);
  });
  hRestore->setMethod(HTTP_POST);
  hRestore->setMaxContentLength(65536);
  server.addHandler(hRestore);

  // ---- system
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) { queue(ACT_REBOOT); sendOk(r); });
  server.on("/api/factory", HTTP_POST, [](AsyncWebServerRequest* r) { queue(ACT_FACTORY); sendOk(r); });
  server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (s_updateOk) { queue(ACT_REBOOT); sendOk(r); }
    else sendErr(r, 500, s_updateErr.length() ? s_updateErr.c_str() : "upload failed");
  }, onUpdateBody);

  // ---- captive portal probes
  static const char* probes[] = { "/generate_204", "/gen_204", "/hotspot-detect.html", "/connecttest.txt", "/ncsi.txt",
                                  "/fwlink", "/redirect", "/success.txt", "/canonical.html", "/library/test/success.html" };
  for (const char* p : probes) {
    server.on(p, HTTP_GET, [](AsyncWebServerRequest* r) {
      if (wifiIsAP()) r->redirect("http://" + WiFi.softAPIP().toString() + "/");
      else r->send(204);
    });
  }

  // ---- static UI
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html").setCacheControl("max-age=300");

  server.onNotFound([](AsyncWebServerRequest* r) {
    if (r->method() == HTTP_OPTIONS) { r->send(200); return; }
    if (r->url().startsWith("/api/")) { sendErr(r, 404, "not found"); return; }
    if (isCaptiveHost(r)) { r->redirect("http://" + WiFi.softAPIP().toString() + "/"); return; }
    if (!LittleFS.exists("/www/index.html")) {
      r->send(200, "text/html", "<h2>LED controller</h2><p>Web UI not installed. Upload the filesystem image "
                                "(<code>pio run -t uploadfs</code>) or use the REST API under /api/.</p>");
      return;
    }
    r->send(404, "text/plain", "Not found");
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  server.begin();
  log_i("HTTP server started");
}

void webRequestFactoryReset() { queue(ACT_FACTORY); }

void webLoop() {
  if (!s_actions) return;
  if (millis() - s_actionAt < 150) return;   // let the HTTP response go out first
  uint32_t a = s_actions; s_actions = 0;

  if (a & ACT_FACTORY) {
    log_w("factory reset");
    LittleFS.remove("/config.json"); LittleFS.remove("/profiles.json");
    Preferences p; p.begin("time", false); p.clear(); p.end();
    WiFi.disconnect(true, true);
    delay(200); ESP.restart();
  }
  if (a & ACT_SAVE_CONFIG)   configSave();
  if (a & ACT_SAVE_PROFILES) profilesSave();
  if (a & ACT_APPLY_CONFIG)  { channelsReconfigure(); engineConfigChanged(); }
  if (a & ACT_TIME_APPLY)    timeApplyConfig();
  if (a & ACT_WIFI_CONNECT)  wifiConnect(s_newSsid, s_newPass);
  if (a & ACT_WIFI_AP)       wifiStartAP();
  if (a & ACT_REBOOT)        { log_w("reboot"); delay(200); ESP.restart(); }
}
