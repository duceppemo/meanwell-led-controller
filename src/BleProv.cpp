#include "BleProv.h"
#if ENABLE_BLE
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "WifiManager.h"

static NimBLECharacteristic* s_status = nullptr;
static String s_ssid, s_pass;
static uint32_t s_lastNotify = 0;

class WriteCb : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    std::string v = c->getValue();
    const NimBLEUUID& id = c->getUUID();
    if (id == NimBLEUUID((uint16_t)0xFFF1)) s_ssid = v.c_str();
    else if (id == NimBLEUUID((uint16_t)0xFFF2)) s_pass = v.c_str();
    else if (id == NimBLEUUID((uint16_t)0xFFF3) && s_ssid.length()) wifiConnect(s_ssid.c_str(), s_pass.c_str());
  }
};

void bleBegin() {
  NimBLEDevice::init(cfg.hostname);
  NimBLEServer* srv = NimBLEDevice::createServer();
  NimBLEService* svc = srv->createService(NimBLEUUID((uint16_t)0xFFF0));
  static WriteCb cb;
  svc->createCharacteristic(NimBLEUUID((uint16_t)0xFFF1), NIMBLE_PROPERTY::WRITE)->setCallbacks(&cb);
  svc->createCharacteristic(NimBLEUUID((uint16_t)0xFFF2), NIMBLE_PROPERTY::WRITE)->setCallbacks(&cb);
  svc->createCharacteristic(NimBLEUUID((uint16_t)0xFFF3), NIMBLE_PROPERTY::WRITE)->setCallbacks(&cb);
  s_status = svc->createCharacteristic(NimBLEUUID((uint16_t)0xFFF4), NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(svc->getUUID());
  adv->setName(cfg.hostname);
  adv->start();
  log_i("BLE provisioning advertising as %s", cfg.hostname);
}

void bleLoop() {
  if (!s_status || millis() - s_lastNotify < 2000) return;
  s_lastNotify = millis();
  JsonDocument d; wifiStatusJson(d.to<JsonObject>());
  String s; serializeJson(d, s);
  s_status->setValue(s.c_str());
  s_status->notify();
}
#else
void bleBegin() {}
void bleLoop() {}
#endif
