# Feature spec and market survey

## Products looked at

| Product | Market | Features worth borrowing |
|---|---|---|
| **Optic LED Master Controller V3** | horticulture | 0–100 % manual dim, dual zone, auto sunrise/sunset with multiple programming slots, **5 saveable modes** (growth phases), humidity-based protective dimming, later add-on sensor probe. Screen + buttons only. |
| **Gavita EL3** | horticulture | High-temperature **auto-dim and shutdown**, overlapping cycles per channel (12/12 + 18/6), "spray mode" (temporary dim), non-volatile settings, logs. |
| **TrolMaster Hydro-X** | horticulture | Two schedule lines, auto dimming, overheat shutdown, sunrise/sunset. App-centric. |
| **TC420 / TC421** | aquarium / DIY | 5 channels, 24 h curve of points per channel, cheap. Clunky programming (PC software / tiny screen), no network on the TC420. |
| **Coralux Storm X** | reef | 16 channels, 12-bit resolution, **clouds and lightning**, sunrise/sunset. |
| **Bluefish Mini** | reef | App control, 6 channels, 3.3 V PWM only (needs level shifting, like us). |
| **Neptune Apex VDM** | reef | 0–10 V ports, sunrise/sunset, **lunar cycle**, clouds, lightning. |
| **Aqua Illumination Hydra / myAI** | reef & planted | Multi-point schedule, **acclimation mode** (start low, ramp to 100 % over days), separate moonlight channel, weather. |
| **Coralvue Hydros** | reef | Multiple moonlight channels, cloud and storm effects. |
| **aquacontrol32 / Borneo-IoT** | open source ESP32 | Web UI, 5 channels, gradual ramps, moon phase simulator; Borneo has sunrise/sunset in ESP-IDF. |

## What this project implements

### Dimming
- Up to **8 PWM channels** (6 on ESP32-C3), 8–14 bit resolution, 100 Hz – 3 kHz, per-channel
  **invert** flag for open-collector drive of DIM+/DIM-.
- Per-channel **min / max %** so the schedule's 1 % maps to the driver's real minimum
  (8–10 %) while 0 % is still off; a **max cap** to derate a fixture.
- Per-channel **gamma** for a perceptual dimming curve.
- Global **master** level.
- **Slew limiting** (max %/s): smooth transitions when switching modes, soft start at boot.
- Optional **relay output** for a true off with older HLG-xxxH-B drivers (which only dim
  to ~10 %) and to cut the drivers' standby draw.

### Scheduling
- **Profiles** ("modes", up to 5) such as Seedling / Veg / Bloom, each with one 24 h curve
  per channel, up to 16 (time, level) points. Curves wrap past midnight, so an 18/6 that
  starts in the evening just works.
- Linear or **cosine ("smooth") interpolation** between points → natural sunrise/sunset
  ramps of any length, mid-day peaks, dusk colour shifts on separate channels.
- **Wizard** to build a classic sunrise → hold → sunset curve in one click, then editable
  point by point on an interactive graph (click to add points).
- **Preview**: play the whole day in 60 s to check a schedule on the real fixture.

### Modes
- **Auto** (schedule), **Manual** (sliders), **Off**, plus **timed pause** (15 min – 4 h) for
  feeding / maintenance / spraying that returns to Auto by itself (Gavita "spray mode").

### Effects (aquarium heritage, all optional, per channel opt-in)
- **Lunar cycle**: moon-flagged channels follow the real moon phase, up to a configurable
  max % at full moon.
- **Passing clouds**: random, smooth dips of configurable depth and frequency.
- **Lightning storms**: triggered on demand (dashboard or API) for N seconds, random groups
  of short full-brightness flashes.

### Protection and biology
- **Acclimation**: new plants / corals start at X % and ramp to 100 % over N days.
- **Thermal protection** with a DS18B20 or a temperature posted from outside: proportional
  dimming between two thresholds, latched shutdown above a third, automatic recovery.
- **Clock loss handling**: time is persisted to NVS every 5 min and restored at boot as
  "approximate" so a power cut without NTP does not leave the lights at a fallback level;
  a configurable fallback level is used only when the time was never known.

### Connectivity
- Wi-Fi **AP-first** setup with captive portal ("LEDctrl-xxxx"), then station mode; AP
  comes back automatically if the home network is lost; **mDNS** (`ledctrl.local`).
- **Web UI** (single page, mobile friendly) served from LittleFS.
- **REST API** (JSON) for everything the UI does → Home Assistant RESTful integration, Node-RED, curl.
- **OTA** firmware and filesystem update from the browser; backup / restore of all settings and schedules as one JSON file.
- Optional **BLE provisioning** build (`esp32dev-ble`): write SSID / password to GATT
  characteristics from a phone app such as nRF Connect.
- NTP with POSIX TZ (DST aware), or clock set from the browser when offline.

## Not implemented (ideas for later)
- MQTT / Home Assistant discovery (REST works today).
- Humidity-based dimming (Optic V3) — humidity can already be posted; add a rule.
- Light-sensor feedback (PAR / lux) for constant-output control.
- Per-weekday schedules; sunrise/sunset from geographic location (needs lat/long + solar math).
- Logging / history graphs.
