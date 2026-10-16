#include "WifiManager.h"
#include "Config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

static DNSServer s_dns;
static bool s_ap = false, s_otaReady = false, s_mdns = false;
static uint32_t s_staAttemptAt = 0, s_apStartedAt = 0, s_connectedAt = 0;
static String s_apSsid;
static const uint32_t STA_TIMEOUT = 20000, STA_RETRY = 120000, AP_LINGER = 180000;

String wifiApSsid() { return s_apSsid; }
bool wifiIsAP() { return s_ap; }
bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

static void startServices() {
  if (!s_mdns) {
    if (MDNS.begin(cfg.hostname)) { MDNS.addService("http", "tcp", 80); s_mdns = true; }
  }
  if (!s_otaReady) {
    ArduinoOTA.setHostname(cfg.hostname);
    ArduinoOTA.begin();
    s_otaReady = true;
  }
}

void wifiStartAP() {
  if (s_ap) return;
  WiFi.mode(WIFI_AP_STA);
  if (cfg.apPass[0]) WiFi.softAP(s_apSsid.c_str(), cfg.apPass);
  else WiFi.softAP(s_apSsid.c_str());
  s_dns.setErrorReplyCode(DNSReplyCode::NoError);
  s_dns.start(53, "*", WiFi.softAPIP());
  s_ap = true; s_apStartedAt = millis();
  log_i("AP started: %s  %s", s_apSsid.c_str(), WiFi.softAPIP().toString().c_str());
}

static void stopAP() {
  if (!s_ap) return;
  s_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  s_ap = false;
  log_i("AP stopped");
}

static void beginSTA() {
  if (!cfg.ssid[0]) return;
  WiFi.setHostname(cfg.hostname);
  WiFi.begin(cfg.ssid, cfg.pass);
  s_staAttemptAt = millis();
  log_i("connecting to %s", cfg.ssid);
}

void wifiBegin() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char b[20]; snprintf(b, sizeof(b), "LEDctrl-%02X%02X", mac[4], mac[5]);
  s_apSsid = b;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  if (cfg.ssid[0]) beginSTA(); else wifiStartAP();
}

void wifiLoop() {
  if (s_ap) s_dns.processNextRequest();
  if (s_otaReady) ArduinoOTA.handle();

  bool up = wifiConnected();
  if (up) {
    if (s_connectedAt == 0) {
      s_connectedAt = millis();
      log_i("STA connected: %s", WiFi.localIP().toString().c_str());
      startServices();
    }
    if (s_ap && millis() - s_apStartedAt > AP_LINGER && millis() - s_connectedAt > 30000) stopAP();
  } else {
    if (s_connectedAt) { s_connectedAt = 0; s_staAttemptAt = millis(); log_w("STA link lost"); }  // grace period before AP
    if (!s_ap && (!cfg.ssid[0] || millis() - s_staAttemptAt > STA_TIMEOUT)) wifiStartAP();
    if (s_ap && cfg.ssid[0] && millis() - s_staAttemptAt > STA_RETRY) beginSTA();
  }
}

void wifiConnect(const char* ssid, const char* pass) {
  strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
  strlcpy(cfg.pass, pass, sizeof(cfg.pass));
  configSave();
  WiFi.disconnect();
  beginSTA();
}

void wifiStatusJson(JsonObject o) {
  o["ap"] = s_ap;
  o["apSsid"] = s_apSsid;
  o["apIp"] = s_ap ? WiFi.softAPIP().toString() : String("");
  o["connected"] = wifiConnected();
  o["ssid"] = cfg.ssid;
  o["ip"] = wifiConnected() ? WiFi.localIP().toString() : String("");
  o["rssi"] = wifiConnected() ? WiFi.RSSI() : 0;
  o["hostname"] = cfg.hostname;
  o["mac"] = WiFi.macAddress();
}

void wifiScanJson(JsonArray a) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true, false); return; }
  if (n == WIFI_SCAN_RUNNING) return;
  for (int i = 0; i < n; i++) {
    JsonObject j = a.add<JsonObject>();
    j["ssid"] = WiFi.SSID(i); j["rssi"] = WiFi.RSSI(i); j["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  WiFi.scanNetworks(true, false);   // keep results fresh for the next call
}
