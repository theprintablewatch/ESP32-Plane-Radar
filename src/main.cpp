/**
 * Plane Radar — WiFi setup, then radar UI on the round GC9A01 display.
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/display.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/radar_rotation.h"
#include "services/sat_client.h"
#include "services/touch_input.h"
#include "services/web_settings.h"
#include "services/wifi_setup.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/sat_display.h"
#include "ui/status_screens.h"

namespace {

// A tap cycles through these views in order; each stays until the next tap.
enum class View { Planes, Satellites, Ip };
View g_view = View::Planes;

bool g_view_drawn = false;  // current view has been rendered at least once
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_adsb_fetch_ms = 0;
unsigned long g_last_sat_fetch_ms = 0;

String g_local_ip_str;

#if defined(BOARD_WAVESHARE_43B)

// 4.3B: GT911 is driven by LovyanGFX; report a tap on the press edge. No swipe
// gestures here — the satellite radar targets the round touch display.
bool s_touch_was_down = false;

void touchInit() {}  // GT911 configured in the LGFX panel

services::touch::Gesture pollTouchGesture() {
  int32_t x = 0;
  int32_t y = 0;
  const bool down = tft.getTouch(&x, &y);
  services::touch::Gesture g = services::touch::Gesture::None;
  if (down && !s_touch_was_down) {
    g = services::touch::Gesture::Tap;
  }
  s_touch_was_down = down;
  return g;
}

#else

// Round display: CST816S capacitive touch over I2C reports taps and swipes.
void touchInit() { services::touch::init(); }

services::touch::Gesture pollTouchGesture() { return services::touch::poll(); }

#endif

void drawCurrentView() {
  if (WiFi.status() != WL_CONNECTED) {
    g_view_drawn = false;
    return;
  }
  switch (g_view) {
    case View::Planes:
      ui::radarDisplayDraw();
      break;
    case View::Satellites:
      ui::satDisplayDraw();
      break;
    case View::Ip:
      g_local_ip_str = WiFi.localIP().toString();
      statusScreenIp(g_local_ip_str.c_str());
      break;
  }
  g_view_drawn = true;
}

const char* viewName(View v) {
  switch (v) {
    case View::Planes: return "planes";
    case View::Satellites: return "satellites";
    case View::Ip: return "ip";
  }
  return "?";
}

// Tap advances Planes -> Satellites -> Ip -> Planes; view holds until next tap.
void advanceView() {
  g_view = static_cast<View>((static_cast<int>(g_view) + 1) % 3);
  Serial.printf("View: %s\n", viewName(g_view));
  // Force an immediate fetch for the newly shown radar.
  g_last_adsb_fetch_ms = 0;
  g_last_sat_fetch_ms = 0;
  drawCurrentView();
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_view_drawn && WiFi.status() == WL_CONNECTED && g_view == View::Planes) {
    ui::radarDisplayDraw();
  }
}

void handleBootButton() {
  bootButtonPollLongPress();
  if (bootButtonConsumeTap()) {
    onRangeTap();
  }
}

void fetchAndDrawAircraft() {
  const float fetch_km = ui::radar::fetchRadiusKm();
  if (!services::adsb::fetchUpdate(services::location::lat(),
                                   services::location::lon(), fetch_km)) {
    handleBootButton();
    return;
  }
  ui::radarDisplayRefreshAircraft();
  handleBootButton();
}

void fetchAndDrawSatellites() {
  if (!services::sat::hasApiKey()) {
    ui::satDisplayShowNoKey();
    handleBootButton();
    return;
  }
  if (services::sat::fetchUpdate(services::location::lat(),
                                 services::location::lon())) {
    ui::satDisplayRefresh();
  }
  handleBootButton();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Plane Radar");

#if !defined(BOARD_WAVESHARE_43B)
  // Round display: enable backlight GPIO. On the 4.3B the backlight is on the
  // CH422G expander and is enabled inside displayInit().
  pinMode(config::kDisplayPinBl, OUTPUT);
  digitalWrite(config::kDisplayPinBl, HIGH);
#endif

  bootButtonInit();
  displayInit();
  touchInit();
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::location::init();
  services::radar_rotation::init();
  services::sat::init();
  ui::radar::rangeInit();

  if (wifiSetupConnect()) {
    services::web_settings::init();
    drawCurrentView();
  }
}

void loop() {
  // A tap (or a swipe) advances to the next view. One physical gesture can emit
  // several events, so debounce: ignore further advances for a short window.
  const services::touch::Gesture gesture = pollTouchGesture();
  const bool advance = gesture == services::touch::Gesture::Tap ||
                       gesture == services::touch::Gesture::SwipeLeft ||
                       gesture == services::touch::Gesture::SwipeRight;
  if (advance) {
    static unsigned long s_last_advance_ms = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - s_last_advance_ms > 800) {
      s_last_advance_ms = millis();
      advanceView();
    }
  }

  handleBootButton();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_view_drawn) {
      Serial.println("WiFi lost — will reconnect");
      g_view_drawn = false;
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        drawCurrentView();
      }
    }
  } else {
    g_wifi_down_since = 0;
    services::web_settings::handle();
    if (!g_view_drawn) {
      drawCurrentView();
    } else if (g_view == View::Planes) {
      if (millis() - g_last_adsb_fetch_ms >= config::kAdsbFetchIntervalMs) {
        g_last_adsb_fetch_ms = millis();
        fetchAndDrawAircraft();
      }
    } else if (g_view == View::Satellites) {
      if (millis() - g_last_sat_fetch_ms >= config::kSatFetchIntervalMs) {
        g_last_sat_fetch_ms = millis();
        fetchAndDrawSatellites();
      }
    }
    // View::Ip is static — nothing to refresh.
  }

  delay(10);
}
