#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

#if defined(BOARD_WAVESHARE_43B)
// RGB parallel bus/panel support is ESP32-S3 only and not pulled in by the
// generic LovyanGFX.hpp auto-detect, so include the platform headers directly.
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>
#endif

#if defined(BOARD_WAVESHARE_43B)

/**
 * LovyanGFX device: Waveshare ESP32-S3-Touch-LCD-4.3B.
 * 800x480 RGB-parallel panel + GT911 capacitive touch.
 * Pins/timing from LovyanGFX discussion #537 (Waveshare 4.3 reference).
 * Backlight + LCD reset are on the CH422G expander (see io_expander_ch422g),
 * so they are not configured here.
 */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_RGB _bus;
  lgfx::Panel_RGB _panel;
  lgfx::Touch_GT911 _touch;

public:
  LGFX() {
    {
      auto cfg = _panel.config();
      cfg.memory_width = config::kDisplayWidth;
      cfg.memory_height = config::kDisplayHeight;
      cfg.panel_width = config::kDisplayWidth;
      cfg.panel_height = config::kDisplayHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel.config(cfg);
    }
    {
      auto cfg = _panel.config_detail();
      cfg.use_psram = 1;  // 800x480x2 framebuffer lives in PSRAM
      _panel.config_detail(cfg);
    }
    {
      auto cfg = _bus.config();
      cfg.panel = &_panel;

      // Blue
      cfg.pin_d0 = 14; cfg.pin_d1 = 38; cfg.pin_d2 = 18; cfg.pin_d3 = 17; cfg.pin_d4 = 10;
      // Green
      cfg.pin_d5 = 39; cfg.pin_d6 = 0;  cfg.pin_d7 = 45; cfg.pin_d8 = 48;
      cfg.pin_d9 = 47; cfg.pin_d10 = 21;
      // Red
      cfg.pin_d11 = 1; cfg.pin_d12 = 2; cfg.pin_d13 = 42; cfg.pin_d14 = 41; cfg.pin_d15 = 40;

      cfg.pin_henable = 5;
      cfg.pin_vsync = 3;
      cfg.pin_hsync = 46;
      cfg.pin_pclk = 7;
      cfg.freq_write = 14000000;  // drop to 12 MHz if the image shimmers

      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 20;
      cfg.hsync_pulse_width = 10;
      cfg.hsync_back_porch = 10;
      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 10;
      cfg.vsync_pulse_width = 10;
      cfg.vsync_back_porch = 10;
      cfg.pclk_active_neg = 0;
      cfg.de_idle_high = 0;
      cfg.pclk_idle_high = 0;
      _bus.config(cfg);
    }
    _panel.setBus(&_bus);

    {
      auto cfg = _touch.config();
      cfg.x_min = 0;
      cfg.x_max = config::kDisplayWidth - 1;
      cfg.y_min = 0;
      cfg.y_max = config::kDisplayHeight - 1;
      cfg.pin_int = static_cast<int>(config::kTouchPinInt);
      cfg.pin_rst = -1;  // GT911 reset is driven via the CH422G expander
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = config::kTouchI2cPort;
      cfg.pin_sda = static_cast<int>(config::kTouchPinSda);
      cfg.pin_scl = static_cast<int>(config::kTouchPinScl);
      cfg.freq = config::kTouchI2cHz;
      cfg.i2c_addr = config::kTouchI2cAddr;  // try 0x5D if touch is dead
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }

    setPanel(&_panel);
  }
};

#else

/** LovyanGFX device: GC9A01 on SPI. Pin values come from config.h. */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
  lgfx::Panel_GC9A01 _panel;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.freq_write = config::kDisplaySpiWriteHz;
      cfg.pin_sclk = static_cast<int>(config::kDisplayPinSclk);
      cfg.pin_mosi = static_cast<int>(config::kDisplayPinMosi);
      cfg.pin_miso = -1;
      cfg.pin_dc = static_cast<int>(config::kDisplayPinDc);
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = static_cast<int>(config::kDisplayPinCs);
      cfg.pin_rst = static_cast<int>(config::kDisplayPinRst);
      cfg.invert = config::kDisplayInvert;
      cfg.rgb_order = config::kDisplayRgbOrder;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

#endif
