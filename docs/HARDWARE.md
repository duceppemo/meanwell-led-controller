# Hardware: driving Mean Well 3-in-1 dimming from an ESP32

## What the driver expects

Mean Well "B" (HLG, ELG) and "AB" (XLG, ELG, HLG) types have a **DIM+ / DIM-** pair
that accepts one of three signals, auto-detected (from the HLG-240H and XLG-240 datasheets):

| Signal | Range | Notes |
|---|---|---|
| DC voltage | 0–10 V (XLG-AB, HLG-C-B) / 1–10 V (older HLG-H-B) | 10 V = 100 % |
| **PWM, 10 V amplitude** | **100 Hz – 3 kHz** | duty cycle = output level. XLG: above 2 kHz the output only starts at 10–15 % duty |
| Resistance | 10 kΩ – 100 kΩ (100 kΩ = 100 %) | the driver sources ~100 µA (typ.) into DIM+ |

Minimum dimming level is about **8 % (XLG)** / **6 % (HLG-C)**; below that the output
current is "not defined". XLG "AB" and HLG "C" B-types **dim to off** at 0 % duty, 0 V or a
short. The older HLG-xxxH-B (non-C, 1–10 V input) units do not dim to off and sit at ~10 %
at 0 V; for those, or to kill the ~0.5 W standby draw, use the optional relay output
(Settings → Relay) to cut mains.

### Datasheet figures for the drivers this project was built around

| Driver | Type | Dimming input | Min. level | Dims to off | Notes |
|---|---|---|---|---|---|
| XLG-100-H-AB | constant power, 100 W | 0–10 V / 10 V PWM 100 Hz–3 kHz / 10–100 kΩ | ~8 % | yes | source current 100 µA |
| XLG-240-M-AB | constant power, 240 W | same | ~8 % | yes | **above 2 kHz PWM the output only starts at 10–15 % duty**, keep ≤ 2 kHz |
| HLG-320H-C1750-B | constant current 1750 mA, 91–183 V | same (0–10 V) | ~6 % | yes | newer "C" series, unlike HLG-xxxH-B |

Set each channel's **Min %** in Settings → Channels to the figure above (8 for the XLG
channels, 6 for the HLG-C channel). The firmware's 1 kHz default suits all three.

The dimming input is isolated from the LED output on the XLG/ELG "AB" types and on the
HLG "B" types (check the block diagram of your exact model). Tie the ESP32 ground to
**DIM-** only, never to the LED output -V.

## Level shifting 3.3 V PWM to the 10 V input

The ESP32 outputs 3.3 V, and the driver interprets 3.3 V as ~33 %. Three proven options:

### A. Open-collector / open-drain (simplest, 1 transistor per channel)

![Open-collector stage: the GPIO drives an NPN through a 1 kΩ base resistor with a 10 kΩ boot pull-up to 3.3 V; the collector goes to DIM+, the emitter to DIM- and ESP32 ground; inside the driver a 100 µA source pulls DIM+ to about 10 V when the transistor is off. Waveforms show the GPIO high 25 % of the time and DIM+ high 75 % with a slow rising edge.](images/open_collector.svg)

Pin order on the TO-92 package, flat face toward you, leads down, left to right:
**BC547 = C B E**, **2N7000 = S G D**. The PN2222A is E B C, the reverse of the BC547,
so it is not a drop-in swap.

The driver's own ~100 µA current source pulls DIM+ to ~10 V when the transistor is off and
the transistor shorts it to 0 V when on. The driver sees a 10 V PWM signal **inverted**
relative to the GPIO: GPIO high = 0 %. Tick **Invert** for the channel in
Settings → Channels so the firmware compensates.

Because the pull-up is only 100 µA, the rising edge is slow (RC of the driver's input).
Keep the PWM frequency at **≤ 1 kHz** with this circuit. If several drivers share one
channel, the current adds up (100 µA × N), which is fine for the transistor.

**Boot state matters.** Until the firmware configures the pins (a few hundred ms after
power-up, longer after a crash) every GPIO floats. With an open-collector stage a floating
GPIO means the transistor is off and the driver runs at **100 %**. Add a **10 kΩ pull-up
from the base/gate to 3.3 V** so the transistor is on (driver at 0 %) whenever the GPIO is
not driven. The firmware also parks every unused channel pin at its "off" level.

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
| esp32dev (WROOM-32) | 25, 26, 27, 14, 32, 33, 13, 23 | 4 | 0 (BOOT) | 2 (on-board LED) |
| **xiao-c3** (Seeed XIAO ESP32-C3) | 3, 4, 5, 6, 7, 10 (D1–D5, D10); 1 channel enabled by default | 21 (D6) | 9 (BOOT) | none; suggested relay pin D7 (GPIO 20) |
| esp32-c3-devkitm-1 | 3, 4, 5, 6, 7, 10 | 1 | 9 (BOOT) | on-board RGB LED (GPIO 8) |
| esp32-s3-devkitc-1 | 4, 5, 6, 7, 15, 16, 17, 18 | 8 | 0 (BOOT) | on-board RGB LED (GPIO 48) |

Pins are only defaults and can be changed from the web UI. Avoid GPIO 0, 2, 12, 15 on
the classic ESP32 for outputs (boot strapping pins) and GPIO 34–39 (input only). On the
C3 avoid GPIO 2, 8, 9 (strapping) and 18/19 (USB).

### XIAO ESP32-C3 build (one board per driver)

With a board at each light bar, each XIAO runs one channel: GPIO 3 (**D1**) drives the
transistor stage, **D6** (GPIO 21) takes the DS18B20, and **D7** (GPIO 20) is free for a
relay. Power it from the 5 V pad or USB-C. Attach the u.FL antenna; the board has no
PCB antenna. There is no user LED, so Wi-Fi state is only visible in the UI and serial
log; the BOOT button still gives the 5 s access-point and 15 s factory-reset gestures.

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
