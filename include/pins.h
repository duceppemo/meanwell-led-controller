#pragma once
// Default pin map per target. Channel pins are only defaults: they are
// editable from the web UI (Settings > Channels) and stored in config.json.
#if defined(BOARD_XIAO_C3)
  // Seeed Studio XIAO ESP32-C3, one board per driver. D0 (GPIO 2) and D8/D9 (GPIO 8/9)
  // are strapping pins: not used for outputs. GPIO 0/1 are not broken out on the XIAO.
  #define MAX_CHANNELS   6
  #define DEFAULT_CH_PINS {3, 4, 5, 6, 7, 10}   // D1, D2, D3, D4, D5, D10
  #define DEFAULT_NUM_CHANNELS 1                // one driver per board; raise in Settings if needed
  #define PIN_ONEWIRE    21    // D6 (suggested relay pin if you add one: D7 = GPIO 20)
  #define PIN_STATUS_LED -1    // the XIAO has no user LED
  #define PIN_BUTTON     9     // BOOT button (D9)
#elif defined(BOARD_ESP32_C3)
  #define MAX_CHANNELS   6
  // GPIO 2, 8, 9 are strapping pins on the C3: not used for outputs.
  #define DEFAULT_CH_PINS {3, 4, 5, 6, 7, 10}
  #define PIN_ONEWIRE    1
  #define PIN_STATUS_LED -1    // DevKitM-1 has an addressable RGB LED on GPIO 8 -> driven via RGB_BUILTIN
  #define STATUS_RGB     1
  #define PIN_BUTTON     9     // BOOT button
#elif defined(BOARD_ESP32_S3)
  #define MAX_CHANNELS   8
  #define DEFAULT_CH_PINS {4, 5, 6, 7, 15, 16, 17, 18}
  #define PIN_ONEWIRE    8
  #define PIN_STATUS_LED -1    // DevKitC-1 has an addressable RGB LED on GPIO 48 -> driven via RGB_BUILTIN
  #define STATUS_RGB     1
  #define PIN_BUTTON     0
#else
  #define MAX_CHANNELS   8
  // GPIO 12 is a strapping pin (MTDI) and GPIO 0/2/15 affect boot: not used for outputs.
  #define DEFAULT_CH_PINS {25, 26, 27, 14, 32, 33, 13, 23}
  #define PIN_ONEWIRE    4
  #define PIN_STATUS_LED 2
  #define PIN_BUTTON     0     // BOOT button: hold 5 s = start AP, 15 s = factory reset
#endif

#ifndef DEFAULT_NUM_CHANNELS
  #define DEFAULT_NUM_CHANNELS 4
#endif

#define MAX_PROFILES   5      // "modes" like seedling / veg / bloom ...
#define MAX_POINTS     16     // schedule points per channel per profile
