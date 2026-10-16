# Mean Well LED controller (ESP32)

Programmable PWM dimming controller for Mean Well **3-in-1 dimmable** LED drivers
(HLG/XLG/ELG "B" and "AB" types) with sunrise/sunset schedules, growth-phase profiles,
lunar / cloud / lightning effects, thermal protection, and a Wi-Fi web interface.
Think "Optic LED Master Controller V3 without the screen", plus the effects found on
reef-aquarium controllers.

- [docs/FEATURES.md](docs/FEATURES.md): market survey and feature list
- [docs/HARDWARE.md](docs/HARDWARE.md): wiring the ESP32 to DIM+/DIM-, pin maps, sensors, relay

## Build and flash

Requires [PlatformIO](https://platformio.org/). The project uses the
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino core 3.x).

```sh
pio run -e esp32dev                 # build firmware
pio run -e esp32dev -t upload       # flash firmware
pio run -e esp32dev -t uploadfs     # flash the web UI (data/www -> LittleFS)
pio device monitor
```

Environments: `esp32dev` (default), `esp32dev-ble` (adds BLE Wi-Fi provisioning),
`esp32-c3`, `esp32-s3`.

Later updates can be done from the browser (System → Firmware update) with
`.pio/build/esp32dev/firmware.bin` or `littlefs.bin` (`pio run -t buildfs`).

## Tests

The web UI's pure helpers (schedule interpolation, time formatting, escaping) have unit
tests that need only Node.js:

```sh
node test/ui/helpers.test.js
```

The firmware has no on-host tests; build all targets with `pio run` to check it compiles.

## First start

1. Power up. The status LED blinks fast: the controller is an access point named
   **LEDctrl-xxxx** (open by default). Join it; the captive portal opens the UI
   (or browse to `http://192.168.4.1`).
2. Settings → Wi-Fi → Scan, pick your network, Connect. Rejoin your home network and open
   `http://ledctrl.local` (or the IP shown by your router). The AP shuts down 3 minutes
   after the station link is up and comes back automatically if the link is lost.
3. Settings → Time: pick your timezone. NTP sync happens automatically; without internet
   use "Set clock from this browser".
4. Settings → Channels: number of channels, GPIO per channel, **Invert** if you use an
   open-collector transistor (see HARDWARE.md), driver minimum %.
5. Schedule: pick a profile, use the wizard or click on the graph to add points, Save,
   Set active. Dashboard → "Preview day" plays the schedule in 60 s.

Hold the BOOT button 5 s to force the access point, 15 s for a factory reset.

## Security

There is **no authentication**: anyone on your LAN (or on the open access point) can
control the lights, read the Wi-Fi password through the backup endpoint, and flash
firmware. Keep the controller on a trusted network, set an access-point password in
Settings → Device, and do not port-forward it. ArduinoOTA (port 3232, no password) is
also enabled, so `pio run -t upload --upload-port ledctrl.local` works over Wi-Fi.

## REST API

All endpoints return JSON. `GET` reads, `POST` writes a JSON body and **requires**
`Content-Type: application/json`.

```sh
curl http://ledctrl.local/api/status
curl -X POST -H "Content-Type: application/json" -d '{"mode":"off","timeoutS":900}' http://ledctrl.local/api/mode
```

| Endpoint | Purpose |
|---|---|
| `GET /api/status` | time, Wi-Fi, sensors, engine state, per-channel levels |
| `GET/POST /api/config` | all settings (channels, PWM, effects, protection, Wi-Fi, time) |
| `GET/POST /api/profiles` | schedules: `{active, profiles:[{name, smooth, channels:[[[minute, level],…],…]}]}` |
| `POST /api/profiles/active` | `{"index": 1}` |
| `POST /api/mode` | `{"mode":"auto"}` · `{"mode":"off","timeoutS":900}` · `{"mode":"manual","levels":[50,50,0,0]}` |
| `POST /api/master` | `{"master": 80}` |
| `POST /api/preview` | `{"seconds": 60}` or `{"stop": true}` |
| `POST /api/storm` | `{"seconds": 60}` or `{"stop": true}` |
| `POST /api/acclimation/start` | `{"start": 50, "days": 14}` |
| `POST /api/sensors` | `{"temp": 42.5, "hum": 55}` from an external system |
| `POST /api/time` | `{"epoch": 1760000000}` |
| `GET /api/wifi/scan`, `POST /api/wifi`, `POST /api/wifi/ap` | Wi-Fi |
| `GET /api/backup`, `POST /api/restore` | full backup as one JSON file |
| `POST /api/update?type=fw|fs` | OTA (multipart upload) |
| `POST /api/reboot`, `POST /api/factory` | system |

Example (Home Assistant `rest_command`):

```yaml
rest_command:
  grow_light_pause:
    url: http://ledctrl.local/api/mode
    method: post
    content_type: application/json
    payload: '{"mode":"off","timeoutS":900}'
```

## Source layout

| File | Role |
|---|---|
| `src/main.cpp` | boot sequence, main loop, button, status LED |
| `src/Config.*` | settings struct, JSON (de)serialisation, LittleFS persistence |
| `src/Schedule.*` | profiles, (minute, level) curves, interpolation |
| `src/Engine.*` | combines schedule + mode + effects + protection into outputs, slew limiting, relay |
| `src/Channels.*` | LEDC PWM outputs, min/max/gamma/invert mapping |
| `src/Effects.*` | moon phase, cloud simulator, lightning simulator |
| `src/TimeKeeper.*` | NTP, POSIX TZ, time persistence across reboots |
| `src/Sensors.*` | DS18B20 and externally posted values |
| `src/WifiManager.*` | AP-first provisioning, captive DNS, mDNS, ArduinoOTA |
| `src/WebUi.*` | async HTTP server, REST API, OTA upload |
| `src/BleProv.*` | optional NimBLE provisioning |
| `data/www/` | web UI (vanilla HTML/CSS/JS) |
| `test/ui/` | Node unit tests for the UI helpers |
| `include/pins.h` | per-board defaults |
