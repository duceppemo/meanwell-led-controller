#pragma once
// Default pin map per target. Channel pins are only defaults: they are
// editable from the web UI (Settings > Channels) and stored in config.json.
#if defined(BOARD_ESP32_C3)
  #define MAX_CHANNELS   6
  #define DEFAULT_CH_PINS {2, 3, 4, 5, 6, 7}
  #define PIN_ONEWIRE    10
  #define PIN_STATUS_LED 8     // on-board LED of the C3 DevKitM-1
  #define PIN_BUTTON     9     // BOOT button
#elif defined(BOARD_ESP32_S3)
  #define MAX_CHANNELS   8
  #define DEFAULT_CH_PINS {4, 5, 6, 7, 15, 16, 17, 18}
  #define PIN_ONEWIRE    8
  #define PIN_STATUS_LED 2
  #define PIN_BUTTON     0
#else
  #define MAX_CHANNELS   8
  // GPIO 12 is a strapping pin (MTDI) and GPIO 0/2/15 affect boot: not used for outputs.
  #define DEFAULT_CH_PINS {25, 26, 27, 14, 32, 33, 13, 23}
  #define PIN_ONEWIRE    4
  #define PIN_STATUS_LED 2
  #define PIN_BUTTON     0     // BOOT button: hold 5 s = start AP, 15 s = factory reset
#endif

#define MAX_PROFILES   5      // "modes" like seedling / veg / bloom ...
#define MAX_POINTS     16     // schedule points per channel per profile
