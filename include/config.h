#pragma once

#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_0;
constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

#if defined(BOARD_WAVESHARE_43B)

// --- Display: Waveshare ESP32-S3-Touch-LCD-4.3B (800×480 RGB parallel) ---
// Bus/panel pins live in hardware/lgfx_config.hpp (driven by LovyanGFX).
// Backlight + LCD/touch reset are on the CH422G IO expander, not GPIO.
constexpr int kDisplayWidth = 800;
constexpr int kDisplayHeight = 480;

// GT911 capacitive touch (I2C, shared bus with the CH422G expander).
constexpr int kTouchI2cPort = 1;
constexpr gpio_num_t kTouchPinSda = GPIO_NUM_8;
constexpr gpio_num_t kTouchPinScl = GPIO_NUM_9;
constexpr gpio_num_t kTouchPinInt = GPIO_NUM_4;
constexpr uint8_t kTouchI2cAddr = 0x14;  // GT911; try 0x5D if touch is dead
constexpr uint32_t kTouchI2cHz = 400000;

// CH422G IO expander: register-style I2C addresses + EXIO output bit map.
// NOTE: the EXIO bit assignments are the most likely thing to need a hardware
// tweak — they come from the Waveshare 4.3B reference, not measured here.
constexpr uint8_t kCh422gAddrMode = 0x24;  // mode register (0x01 = push-pull out)
constexpr uint8_t kCh422gAddrOut = 0x38;   // output register
constexpr uint8_t kCh422gBitTouchRst = 0x01;  // EXIO1 -> TP_RST
constexpr uint8_t kCh422gBitBacklight = 0x04;  // EXIO3 -> LCD backlight
constexpr uint8_t kCh422gBitLcdRst = 0x08;     // EXIO4 -> LCD_RST

// GC9A01-only flags kept defined so shared UI code compiles unchanged.
constexpr bool kDisplayInvert = false;
constexpr bool kDisplayRgbOrder = false;  // RGB panel, no R/B swap

#else

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_14;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_9;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_8;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_11;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_10;  // display SCL
constexpr gpio_num_t kDisplayPinBl = GPIO_NUM_2;     // display backlight

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
// GC9A01 modules often need invert + BGR for correct black/green output
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

// CST816S capacitive touch (Waveshare ESP32-S3-Touch-LCD-1.28).
// Swipe left/right switches between the plane and satellite radars.
constexpr gpio_num_t kTouchPinSda = GPIO_NUM_6;
constexpr gpio_num_t kTouchPinScl = GPIO_NUM_7;
constexpr gpio_num_t kTouchPinInt = GPIO_NUM_5;
constexpr gpio_num_t kTouchPinRst = GPIO_NUM_13;
constexpr uint8_t kTouchI2cAddr = 0x15;
constexpr uint32_t kTouchI2cHz = 400000;

#endif

// --- Radar center defaults (overridden via WiFi setup portal) ---
constexpr double kDefaultRadarLat = 52.3676;
constexpr double kDefaultRadarLon = 4.9041;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** Legacy scale unused — fetch uses radar::fetchRadiusKm() to screen edge. */
constexpr float kAdsbFetchRadiusScale = 1.0f;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;

// --- Satellites (N2YO "above" API; swipe left/right to reach this radar) ---
/** Free N2YO API key (register at https://www.n2yo.com/api/). Empty = set it
 *  later from the WiFi setup portal; the satellite radar prompts until it is. */
constexpr char kN2yoDefaultApiKey[] = "";
/** Poll N2YO at most this often while the satellite radar is showing. */
constexpr unsigned long kSatFetchIntervalMs = 15000;
/** Sky-cone half-angle from the zenith to search (degrees, 0-90). */
constexpr int kSatSearchRadiusDeg = 70;
/** Default N2YO category id (overridable from the setup portal). Avoid 0
 *  (= all): it returns hundreds of objects, far more JSON than the ESP32 can
 *  parse. 1 = brightest/visible is a good, small default. */
constexpr int kSatCategory = 1;
/** Only plot satellites at or above this elevation above the horizon (deg). */
constexpr float kSatMinElevationDeg = 0.0f;

/** Curated N2YO categories offered in the setup portal (id + label). 0 (= all)
 *  is deliberately omitted: its response is too large for the ESP32. */
struct SatCategory {
  int id;
  const char* name;
};
constexpr SatCategory kSatCategories[] = {
  {1, "Brightest"},
  {2, "ISS"},
  {18, "Amateur radio"},
  {52, "Starlink"},
  {53, "OneWeb"},
  {20, "GPS"},
  {22, "Galileo"},
  {15, "Iridium"},
  {3, "Weather"},
  {4, "NOAA"},
  {32, "CubeSats"},
  {26, "Science"},
};
constexpr size_t kSatCategoryCount =
    sizeof(kSatCategories) / sizeof(kSatCategories[0]);

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

// --- Airports ---
struct Airport {
  const char* code;
  double lat;
  double lon;
};

constexpr Airport kAirports[] = {
  {"LHR", 51.4700, -0.4543}, // London Heathrow
  {"LGW", 51.1481, -0.1903}, // London Gatwick
  {"MAN", 53.3539, -2.2750}, // Manchester
  {"STN", 51.8850, 0.2350},  // London Stansted
  {"LTN", 51.8747, -0.3683}, // London Luton
  {"BHX", 52.4539, -1.7481}, // Birmingham
  {"BRS", 51.3828, -2.7192}, // Bristol
  {"NCL", 55.0375, -1.6917}, // Newcastle
  {"LBA", 53.8659, -1.6606}, // Leeds Bradford
  {"EMA", 52.8311, -1.3281}, // East Midlands
  {"LCY", 51.5053, 0.0553},  // London City
  {"SOU", 50.9503, -1.3567}, // Southampton
  {"BOH", 50.7800, -1.8297}, // Bournemouth
  {"LPL", 53.3336, -2.8497}, // Liverpool John Lennon
  {"NWI", 52.6758, 1.2828},  // Norwich
  {"EXT", 50.7344, -3.4139}, // Exeter
  {"SEN", 51.5714, 0.6956},  // London Southend
  {"MME", 54.5092, -1.4294}, // Teesside
  {"NQY", 50.4406, -4.9953}, // Newquay / Cornwall
  {"HUY", 53.5744, -0.3508}  // Humberside
};
constexpr size_t kAirportCount = sizeof(kAirports) / sizeof(kAirports[0]);

}  // namespace config
