# Mean Well LED Controller

Programmable PWM dimming controller for Mean Well **3-in-1 dimmable** LED drivers
(HLG / XLG / ELG "B" and "AB" types) on an ESP32: sunrise and sunset schedules,
growth-phase profiles, lunar / cloud / lightning effects, thermal protection and a
Wi-Fi web interface. Think "Optic LED Master Controller V3 without the screen", plus
the effects found on reef-aquarium controllers.

## Current architecture

![Firmware architecture: Schedule, Effects, Sensors and TimeKeeper feed the Engine, which computes one target level per channel and slews the Channels (LEDC PWM) that drive the Mean Well DIM+/DIM- inputs and an optional relay; WebUi, WifiManager and BleProv drive dashed configuration paths into Config, Schedule and Engine; Config and Schedule persist to LittleFS](docs/images/architecture_schematic.svg)

`src/main.cpp` is intentionally small: it mounts the filesystem, loads config and
profiles, puts every PWM output at 0 %, then polls each subsystem from one loop. The
accent-colored path is the level computation from schedule to driver; dashed lines are
configuration paths (web UI, BLE, the BOOT button). HTTP handlers never touch hardware
directly: they queue actions that the main loop applies. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the component breakdown and the
per-channel formula.

### Wiring

![Wiring: a 5 V supply powers the ESP32; each PWM GPIO drives a 1 kΩ base resistor into an NPN transistor whose collector goes to the driver's DIM+ and emitter to DIM- and ESP32 ground, with a 10 kΩ pull-up from base to 3.3 V so the driver sits at 0 % while the GPIO floats at boot and an optional 10 kΩ pull-up from DIM+ to 12 V for sharper edges; a DS18B20 with 4.7 kΩ pull-up on the 1-Wire pin; an optional relay on another GPIO breaks the drivers' AC line; the driver's +V/-V feeds the LED fixture](docs/images/wiring_diagram.svg)

Mean Well's DIM+ / DIM- input accepts a **10 V PWM signal at 100 Hz – 3 kHz**, and the
driver itself sources about 100 µA into DIM+. One NPN (or small MOSFET) per channel in
open-collector configuration therefore gives a 0–10 V PWM from a 3.3 V GPIO with no
extra supply; the firmware's per-channel **Invert** flag compensates for the inversion.
The base pull-up matters: a floating GPIO at boot would otherwise leave the transistor
off and the driver at 100 %. XLG "AB" and HLG "C" B-types dim to off (minimum 8 % / 6 %);
the older HLG-xxxH-B units stop at ~10 %, which is what the optional relay is for. See
[docs/HARDWARE.md](docs/HARDWARE.md)
for the datasheet figures, alternative level-shifting stages (12 V pull-up,
optocoupler), pin maps per board and the sensor and relay hookups.

### Web UI

A single-page app served from LittleFS, usable from a phone over the controller's own
access point or from the LAN: live channel bars and manual sliders, an interactive 24 h
curve editor with a sunrise/sunset wizard, effects and protection settings, Wi-Fi
onboarding, OTA update and backup/restore. Everything the UI does is also a JSON
endpoint under `/api/`. See [docs/WEB_UI.md](docs/WEB_UI.md).

## Implemented features

- Up to 8 LEDC PWM channels (6 on ESP32-C3), 8–14 bit, 100 Hz – 3 kHz, per-channel
  invert, driver minimum / maximum %, gamma curve, global master, slew-rate limiting,
  unused pins parked at "off"
- Up to 5 profiles (growth phases) with one 24 h curve per channel of up to 16 points,
  linear or cosine interpolation, curves that wrap past midnight, sunrise/sunset wizard,
  60 s whole-day preview on the real fixture
- Modes Auto / Manual / Off plus timed pause (feeding, maintenance, spraying) that
  returns to Auto by itself
- Lunar cycle from the real moon phase, random passing clouds, on-demand lightning
  storms, each opt-in per channel
- Acclimation ramp for new plants or corals (start % → 100 % over N days)
- Thermal protection from a DS18B20 or an externally posted temperature: proportional
  dimming between two thresholds, latched shutdown, automatic recovery
- Optional relay / SSR output for a true off with older HLG-B drivers and to cut standby power
- Clock persisted to NVS every 5 min so a power cut without NTP resumes the schedule at
  approximately the right time; configurable fallback level when time was never known
- AP-first Wi-Fi onboarding with captive portal, automatic fallback to AP when the home
  network is lost, mDNS (`ledctrl.local`), optional BLE provisioning build
- REST API for everything (Home Assistant RESTful integration, Node-RED, curl), browser
  OTA for firmware and UI, ArduinoOTA, one-file backup and restore
- Builds for classic ESP32, ESP32-C3 and ESP32-S3; Node unit tests for the UI's curve
  math

See [docs/FEATURES.md](docs/FEATURES.md) for the market survey behind these choices and
the ideas not implemented yet.

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — design rules, runtime flow and
  component responsibilities
- [docs/HARDWARE.md](docs/HARDWARE.md) — Mean Well dimming specs, level-shifting
  circuits, pin maps, temperature sensor and relay wiring
- [docs/WEB_UI.md](docs/WEB_UI.md) — first start, UI pages, security, REST API with
  Home Assistant examples, BLE provisioning
- [docs/BUILD.md](docs/BUILD.md) — PlatformIO environments, flashing, OTA, tests and
  toolchain notes
- [docs/FEATURES.md](docs/FEATURES.md) — survey of comparable controllers and the
  resulting feature list / roadmap
