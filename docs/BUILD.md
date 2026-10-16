# Build, flash and test

## Toolchain

- [PlatformIO](https://platformio.org/) Core 6.x (CLI or the VS Code extension).
- Platform: [pioarduino/platform-espressif32](https://github.com/pioarduino/platform-espressif32)
  release 55.03.31x (Arduino-ESP32 core 3.3.x), pinned by URL in `platformio.ini`.
  The upstream `platformio/espressif32` platform is stuck on core 2.x and will not build
  this project (it uses the core 3 LEDC API `ledcAttach` / `ledcOutputInvert`).
- Libraries are fetched automatically: ESP32Async/ESPAsyncWebServer + AsyncTCP,
  ArduinoJson 7, OneWire, DallasTemperature, and NimBLE-Arduino 2.x for the BLE env.

## Environments

| env | board | notes |
|---|---|---|
| `esp32dev` (default) | classic ESP32 WROOM-32 | 8 channels |
| `esp32dev-ble` | same | adds NimBLE Wi-Fi provisioning (+250 KB flash) |
| `esp32-c3` | ESP32-C3 DevKitM-1 | 6 channels, USB CDC console |
| `esp32-s3` | ESP32-S3 DevKitC-1 | 8 channels, USB CDC console |

```sh
pio run                              # build default env
pio run -e esp32-c3                  # build another env
pio run -t upload                    # flash firmware over USB
pio run -t uploadfs                  # flash the web UI (data/ -> LittleFS)
pio run -t buildfs                   # only build littlefs.bin
pio device monitor                   # 115200 baud, exception decoder enabled
```

Both `upload` and `uploadfs` are needed on a fresh board: the firmware serves the UI from
the LittleFS partition. Without it the root URL shows a hint page and the API still works.

### Windows note

The RISC-V toolchain installer used by the `esp32-c3` env refuses to run from Git Bash /
MSYS ("MSys/Mingw is not supported"). Run PlatformIO from PowerShell, cmd or the VS Code
PlatformIO toolbar for that target. The Xtensa targets build from any shell.

## Over-the-air updates

- **Browser:** System → Firmware update. Upload `.pio/build/<env>/firmware.bin` as
  *Firmware* or `.pio/build/<env>/littlefs.bin` as *Web UI filesystem*. The device reboots
  when the upload completes.
- **PlatformIO (ArduinoOTA, port 3232):**
  `pio run -t upload --upload-port ledctrl.local` (or the IP). Only available once the
  controller is on your Wi-Fi.

Both OTA paths are unauthenticated; see the Security section of
[WEB_UI.md](WEB_UI.md).

## Versioning

`FW_VERSION` is set in `platformio.ini` (`build_flags`) and reported by `/api/status`
and the System tab. Bump it before publishing a release binary.

## Tests

The firmware itself has no on-host tests; `pio run -e esp32dev -e esp32dev-ble -e esp32-c3 -e esp32-s3`
is the compile check for every target.

The web UI's pure helpers (schedule interpolation, time formatting, HTML escaping) are
unit-tested with Node.js only, no npm packages:

```sh
node test/ui/helpers.test.js
```

The test also runs `node --check` on `data/www/app.js`. The interpolation cases mirror
`scheduleLevelAt()` in `src/Schedule.cpp` so the graph in the browser matches what the
firmware outputs.
