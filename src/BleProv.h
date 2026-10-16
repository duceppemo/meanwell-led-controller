#pragma once
// Optional BLE Wi-Fi provisioning (build with -D ENABLE_BLE=1, env esp32dev-ble).
// Service 0xFFF0: write SSID to 0xFFF1, password to 0xFFF2, write any byte to
// 0xFFF3 to connect; 0xFFF4 is a readable/notifying JSON status string.
void bleBegin();
void bleLoop();
