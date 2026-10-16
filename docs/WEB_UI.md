# Web UI, Wi-Fi setup and REST API

## First start

1. Power up. The status LED blinks fast: the controller is an open access point named
   **LEDctrl-xxxx**. Join it; the captive portal opens the UI (or browse to
   `http://192.168.4.1`).
2. **Settings → Wi-Fi → Scan**, pick your network, enter the password, **Connect**.
   Rejoin your home network and open `http://ledctrl.local` (or the IP from your router).
   The AP closes 3 minutes after the station link is up and comes back automatically if
   the link is lost for more than 20 s, or when you hold the BOOT button for 5 s.
3. **Settings → Time**: pick your timezone. NTP syncs automatically; without internet use
   *Set clock from this browser*.
4. **Settings → Channels**: number of channels, GPIO per channel, **Invert** when using an
   open-collector transistor (see [HARDWARE.md](HARDWARE.md)), the driver's minimum % and
   an optional maximum.
5. **Schedule**: pick a profile, use the wizard or click on the graph to add points,
   **Save**, **Set active**. **Dashboard → Preview day** plays the curve in 60 s on the
   real fixture.

Hold BOOT for 15 s to factory-reset (erases config, schedules and Wi-Fi credentials).

## Pages

| Tab | What it does |
|---|---|
| **Dashboard** | Live bars per channel (output, target marker, driver %), mode buttons Auto / Manual / Off, manual sliders, timed pause (15 min – 4 h, returns to Auto), master level, Preview day, Storm trigger, current moon / cloud / thermal / acclimation / relay state. |
| **Schedule** | Profiles (up to 5, e.g. Seedling / Veg / Bloom): rename, add, delete, set active. Interactive 24 h graph of all channels, click to add a point, point table with time and level, smooth (cosine) toggle, sunrise/sunset wizard, copy curve to all channels. |
| **Effects** | Lunar cycle (max % at full moon, per-channel opt-in), passing clouds (depth, min/max gap), lightning enable, acclimation (start %, days, start now), thermal protection thresholds. |
| **Settings** | Device name, hostname, AP password, Wi-Fi scan and connect, timezone and NTP, PWM frequency / resolution, slew rate, fallback level, relay pin and delay, 1-Wire pin, channel table (enable, name, GPIO, invert, min, max, gamma, moon, effects). |
| **System** | Firmware version, uptime, heap, IP / MAC, OTA upload (firmware or filesystem), backup download and restore, reboot, start access point, factory reset. |

The UI polls `/api/status` every 2 s (0.7 s during preview) and keeps working from the
access point and from the LAN at the same time.

## Security

There is **no authentication**. Anyone on your LAN, or on the access point while it is
open, can control the lights, download the backup (which contains the Wi-Fi password in
clear text) and flash firmware. Mitigations:

- Set an **access point password** (Settings → Device) so the fallback AP is not open.
- Keep the controller on a trusted network; never port-forward it.
- The API sends no CORS headers, so web pages from other origins cannot call it from your
  browser.
- ArduinoOTA (port 3232) is enabled without a password once on Wi-Fi.

## REST API

All endpoints return JSON. `GET` reads; `POST` writes a JSON body and **requires**
`Content-Type: application/json` (requests without it are not matched and return 404).

```sh
curl http://ledctrl.local/api/status
curl -X POST -H "Content-Type: application/json" \
     -d '{"mode":"off","timeoutS":900}' http://ledctrl.local/api/mode
```

| Endpoint | Body / result |
|---|---|
| `GET /api/status` | `fw`, `uptimeS`, `heap`, `time{now,epoch,synced,approx,known,minute,tz}`, `wifi{...}`, `sensors{temp,hum,local}`, `engine{mode,modeTimeoutS,profile,thermal,acclimation,cloud,shutdown,flash,relay,stormS,moon,moonPhase,channels[{name,enabled,target,output,driver,manual}]}`, `master` |
| `GET /api/config` | all settings (passwords masked as `********`) plus `maxChannels`, `maxProfiles`, `maxPoints` |
| `POST /api/config` | same shape; unknown keys ignored, missing keys keep their value, masked passwords are not overwritten |
| `GET /api/profiles` | `{active, profiles:[{name, smooth, channels:[[[minute, level], …], …]}]}` |
| `POST /api/profiles` | same shape, replaces all profiles |
| `POST /api/profiles/active` | `{"index": 1}` |
| `POST /api/mode` | `{"mode":"auto"}` · `{"mode":"off","timeoutS":900}` · `{"mode":"manual","levels":[50,50,0,0],"timeoutS":0}` |
| `POST /api/master` | `{"master": 80}` |
| `POST /api/preview` | `{"seconds": 60}` or `{"stop": true}` |
| `POST /api/storm` | `{"seconds": 60}` or `{"stop": true}` (lightning must be enabled) |
| `POST /api/acclimation/start` | `{"start": 50, "days": 14}` — enables and restarts acclimation now |
| `POST /api/sensors` | `{"temp": 42.5, "hum": 55}` from an external system; used for 10 min |
| `POST /api/time` | `{"epoch": 1760000000}` (UTC seconds) |
| `GET /api/wifi/scan` | `{networks:[{ssid,rssi,secure}]}`; first call starts a scan and may return an empty list |
| `POST /api/wifi` | `{"ssid": "...", "pass": "..."}` saves and connects |
| `POST /api/wifi/ap` | start the access point |
| `GET /api/backup` | config (with secrets) + profiles as one JSON file |
| `POST /api/restore` | that file, or any subset `{config}` / `{profiles}` |
| `POST /api/update?type=fw\|fs` | multipart upload of `firmware.bin` or `littlefs.bin`; reboots on success |
| `POST /api/reboot`, `POST /api/factory` | system |

### Home Assistant example

```yaml
rest_command:
  grow_light_pause:
    url: http://ledctrl.local/api/mode
    method: post
    content_type: application/json
    payload: '{"mode":"off","timeoutS":900}'
  grow_light_temp:
    url: http://ledctrl.local/api/sensors
    method: post
    content_type: application/json
    payload: '{"temp": {{ states("sensor.tent_temperature") }} }'

sensor:
  - platform: rest
    name: Grow light channel 1
    resource: http://ledctrl.local/api/status
    value_template: "{{ value_json.engine.channels[0].output | round(0) }}"
    unit_of_measurement: "%"
```

## BLE provisioning (optional build)

With the `esp32dev-ble` environment the controller advertises as its hostname with
service `0xFFF0`. Using nRF Connect or similar: write the SSID to `0xFFF1`, the password
to `0xFFF2`, then any byte to `0xFFF3`. `0xFFF4` notifies the Wi-Fi status JSON every 2 s.
