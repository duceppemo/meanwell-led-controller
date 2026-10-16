# Hardware: driving Mean Well 3-in-1 dimming from an ESP32

## What the driver expects

Mean Well "B" (HLG, ELG) and "AB" (XLG, ELG, HLG) types have a **DIM+ / DIM-** pair
that accepts one of three signals, auto-detected (from the HLG-240H and XLG-240 datasheets):

| Signal | Range | Notes |
|---|---|---|
| DC voltage | 1–10 V (HLG-B) / 0–10 V (XLG-AB) | 10 V = 100 % |
| **PWM, 10 V amplitude** | **100 Hz – 3 kHz** | duty cycle = output level. XLG: above 2 kHz the output only starts at 10–15 % duty |
| Resistance | 10 kΩ – 100 kΩ (100 kΩ = 100 %) | the driver sources ~100 µA (typ.) into DIM+ |

Minimum dimming level is about **8 % (XLG)** / **10 % (HLG-B)**; below that the output
current is "not defined". The XLG/ELG "AB" types **dim to off** at 0 % duty, 0 V or a short.
HLG-B types do **not** dim to off: at 0 V they sit at ~10 %. Use the optional relay output
(Settings → Relay) to cut mains or DC for a true off.

DIM+/DIM- is isolated from the LED output on these series, so the ESP32 ground can be tied
to DIM- directly.

## Level shifting 3.3 V PWM to the 10 V input

The ESP32 outputs 3.3 V, and the driver interprets 3.3 V as ~33 %. Three proven options:

### A. Open-collector / open-drain (simplest, 1 transistor per channel)

```
ESP32 GPIO ──[1 kΩ]──┤ B   NPN (2N2222 / BC547)  or  2N7000 MOSFET gate (no resistor needed)
                      E ──────────────┬──── DIM-  (and ESP32 GND)
                      C ──────────────┴──── DIM+
```

The driver's own ~100 µA current source pulls DIM+ to ~10 V when the transistor is off and
the transistor shorts it to 0 V when on. The driver sees a 10 V PWM signal **inverted**
relative to the GPIO: GPIO high = 0 %. Tick **Invert** for the channel in
Settings → Channels so the firmware compensates.

Because the pull-up is only 100 µA, the rising edge is slow (RC of the driver's input).
Keep the PWM frequency at **≤ 1 kHz** with this circuit. If several drivers share one
channel, the current adds up (100 µA × N), which is fine for the transistor.

### B. Open-collector with an external 12 V pull-up (clean edges, many drivers per channel)

Same as A, plus a **10 kΩ resistor from DIM+ to +12 V** (any 12 V supply whose ground is
tied to DIM-). Gives a crisp 0–12 V PWM; the drivers' dimming input is rated to accept
it. Still inverted → tick Invert.

### C. Optocoupler (galvanic isolation, e.g. long cable runs or multiple mains drivers)

```
ESP32 GPIO ──[330 Ω]── PC817 anode ... cathode ── GND
PC817 collector ── DIM+  with 10 kΩ pull-up to +12 V (isolated supply, GND to DIM-)
PC817 emitter   ── DIM-
```

Also inverted. A PC817 is slow (~µs); stay at ≤ 1 kHz.

### Non-inverting alternative
A 74HCT125/74AHCT125 buffer at 5 V followed by a MOSFET with 12 V pull-up gives
non-inverted drive, but the firmware's Invert flag makes options A–C just as good.

## Default pin maps

| Target | PWM channels (GPIO) | 1-Wire | Button | Status LED |
|---|---|---|---|---|
| esp32dev (WROOM-32) | 25, 26, 27, 14, 32, 33, 13, 23 | 4 | 0 (BOOT) | 2 |
| esp32-c3-devkitm-1 | 2, 3, 4, 5, 6, 7 | 10 | 9 (BOOT) | 8 |
| esp32-s3-devkitc-1 | 4, 5, 6, 7, 15, 16, 17, 18 | 8 | 0 (BOOT) | 2 |

Pins are only defaults and can be changed from the web UI. Avoid GPIO 0, 2, 12, 15 on
the classic ESP32 for outputs (boot strapping pins) and GPIO 34–39 (input only).

## Temperature sensor

A **DS18B20** on the 1-Wire pin (4.7 kΩ pull-up to 3.3 V) enables heat-sink or canopy
thermal protection: gradual dimming between two thresholds and a latched shutdown. Any
external system can also post a temperature / humidity to `POST /api/sensors`.

## Relay (optional)

Any spare GPIO can drive a relay module (or SSR) that switches the drivers' AC input. The
firmware turns it on when any channel is above 0 % and off after a configurable delay once
everything is at 0 %. Use an opto-isolated relay module; drive it through its IN pin.

## Power

Power the ESP32 from a separate 5 V supply (or a small buck converter from the 12 V
pull-up supply). Do **not** power it from the LED output.

## Button

- Hold BOOT for **5 s**: start the Wi-Fi access point (status LED blinks fast).
- Hold for **15 s**: factory reset (erase config, schedules, Wi-Fi credentials).
