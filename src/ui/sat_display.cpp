#include "ui/sat_display.h"

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cmath>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/radar_rotation.h"
#include "services/sat_client.h"
#include "ui/radar_theme.h"

namespace fonts = lgfx::v1::fonts;

namespace ui {

namespace {

constexpr float kDegToRad = 0.01745329252f;
constexpr size_t kMaxLabels = 8;  // only label the highest satellites
constexpr int kSatTrackLenPx = radar::scaledPx(11);  // travel-vector length

/** Logical RGB -> panel colour, swapping R/B on the GC9A01's BGR order. */
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return config::kDisplayRgbOrder ? tft.color565(b, g, r)
                                  : tft.color565(r, g, b);
}

/** Blue grid/crosshairs colour for the satellite sky plot. */
uint16_t gridBlue() { return rgb(40, 110, 255); }

// VLW sizes resolved to match the plane radar's label heights (computed once
// the smooth font is loaded, so sat text is the same size as aircraft tags).
bool s_sizes_ready = false;
float s_label_vlw = 0.5f;
float s_cardinal_vlw = 0.56f;

int measureVlwHeight(float size) {
  tft.setTextSize(size);
  return tft.fontHeight();
}

float findVlwSizeForHeight(int target_px) {
  float lo = 0.25f;
  float hi = 1.2f;
  for (int i = 0; i < 16; ++i) {
    const float mid = (lo + hi) * 0.5f;
    if (measureVlwHeight(mid) < target_px) {
      lo = mid;
    } else {
      hi = mid;
    }
  }
  return hi;
}

void ensureSizes() {
  if (s_sizes_ready || !displayFontIsSmooth()) {
    return;
  }
  s_label_vlw = findVlwSizeForHeight(radar::kAircraftTagLabelHeightPx);
  s_cardinal_vlw = findVlwSizeForHeight(radar::kCardinalLabelHeightPx);
  s_sizes_ready = true;
}

void applyLabelFont() {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, s_label_vlw);
  } else {
    displayFontSetBitmap(tft, &fonts::FreeSansBold12pt7b);
  }
}

void applyCardinalFont() {
  if (displayFontIsSmooth()) {
    displayFontSetSmoothSize(tft, s_cardinal_vlw);
  } else {
    displayFontSetBitmap(tft, &fonts::FreeSansBold12pt7b);
  }
}

/** Map azimuth/elevation to a screen pixel: elevation -> radius (zenith at
 *  centre, horizon at the outer ring), azimuth -> bearing around the rim,
 *  rotated to match the plane radar's top heading. */
void skyToScreen(float az, float el, int* x, int* y) {
  const float th = services::radar_rotation::topHeading();
  float r = radar::kGridOuterRadius * (90.0f - el) / 90.0f;
  if (r < 0.0f) {
    r = 0.0f;
  }
  const float a = (az - th) * kDegToRad;
  *x = radar::kCenterX + static_cast<int>(lroundf(sinf(a) * r));
  *y = radar::kCenterY - static_cast<int>(lroundf(cosf(a) * r));
}

/** Filled triangle centred at (x,y) with its nose along the unit vector
 *  (ux,uy) — same shape as the plane radar's aircraft symbol. */
void drawTravelTriangle(int x, int y, float ux, float uy, uint16_t color) {
  const int tip_x = x + static_cast<int>(lroundf(ux * radar::kAircraftNoseLenPx));
  const int tip_y = y + static_cast<int>(lroundf(uy * radar::kAircraftNoseLenPx));
  const int base_x = x - static_cast<int>(lroundf(ux * radar::kAircraftTailLenPx));
  const int base_y = y - static_cast<int>(lroundf(uy * radar::kAircraftTailLenPx));
  const float perp_x = -uy;
  const float perp_y = ux;
  const int wx = static_cast<int>(lroundf(perp_x * radar::kAircraftTailHalfPx));
  const int wy = static_cast<int>(lroundf(perp_y * radar::kAircraftTailHalfPx));
  tft.fillTriangle(tip_x, tip_y, base_x + wx, base_y + wy, base_x - wx,
                   base_y - wy, color);
}

/** Unit travel direction in screen space: from the previous sky position to
 *  the current one. Falls back to "radially outward from the zenith" (and then
 *  straight up) when there is no previous sample yet. */
void travelDir(const services::sat::Satellite& sat, int x, int y, float* ux,
               float* uy) {
  float dx = 0.0f;
  float dy = 0.0f;
  if (sat.has_prev) {
    int px = 0;
    int py = 0;
    skyToScreen(sat.prev_az_deg, sat.prev_el_deg, &px, &py);
    dx = static_cast<float>(x - px);
    dy = static_cast<float>(y - py);
  }
  if (dx * dx + dy * dy < 1.0f) {  // no/!tiny motion -> point away from zenith
    dx = static_cast<float>(x - radar::kCenterX);
    dy = static_cast<float>(y - radar::kCenterY);
  }
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.5f) {  // at the zenith -> point up
    *ux = 0.0f;
    *uy = -1.0f;
    return;
  }
  *ux = dx / len;
  *uy = dy / len;
}

void drawGrid() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;
  const int R = radar::kGridOuterRadius;
  const float th = services::radar_rotation::topHeading();

  tft.fillScreen(radar::kColorBackground);

  const uint16_t grid = gridBlue();

  // Compass spokes under the rings.
  for (int b = 0; b < 360; b += 90) {
    const float a = (b - th) * kDegToRad;
    const int ex = cx + static_cast<int>(lroundf(sinf(a) * R));
    const int ey = cy - static_cast<int>(lroundf(cosf(a) * R));
    tft.drawLine(cx, cy, ex, ey, grid);
  }

  // Elevation rings: horizon (outer), 30 deg, 60 deg.
  for (int el = 0; el < 90; el += 30) {
    const int r = static_cast<int>(lroundf(R * (90.0f - el) / 90.0f));
    tft.drawCircle(cx, cy, r, grid);
  }

  // Zenith marker (straight up).
  tft.fillSmoothCircle(cx, cy, radar::kCenterDotRadius, radar::kColorCenter);

  // Cardinal labels around the rim.
  applyCardinalFont();
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(radar::kColorLabel, radar::kColorBackground);
  const char* cardinals[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; ++i) {
    const float a = (i * 90.0f - th) * kDegToRad;
    const int lx = cx + static_cast<int>(lroundf(sinf(a) * (R + 6)));
    const int ly = cy - static_cast<int>(lroundf(cosf(a) * (R + 6)));
    tft.drawString(cardinals[i], lx, ly);
  }
}

void drawSatellites() {
  const size_t n = services::sat::count();
  const services::sat::Satellite* sats = services::sat::list();
  if (n == 0) {
    applyLabelFont();
    tft.setTextDatum(textdatum_t::middle_center);
    tft.setTextColor(radar::kColorLabel, radar::kColorBackground);
    tft.drawString("No satellites", radar::kCenterX,
                   radar::kCenterY + radar::kGridOuterRadius / 2);
    return;
  }

  // Order indices by elevation (highest first) for label priority.
  size_t idx[services::sat::kMaxSatellites];
  for (size_t i = 0; i < n; ++i) {
    idx[i] = i;
  }
  for (size_t i = 1; i < n; ++i) {
    const size_t key = idx[i];
    size_t j = i;
    while (j > 0 && sats[idx[j - 1]].el_deg < sats[key].el_deg) {
      idx[j] = idx[j - 1];
      --j;
    }
    idx[j] = key;
  }

  // Symbols: travel-vector line, then the heading triangle (same as planes).
  for (size_t i = 0; i < n; ++i) {
    int x = 0;
    int y = 0;
    skyToScreen(sats[i].az_deg, sats[i].el_deg, &x, &y);
    float ux = 0.0f;
    float uy = 0.0f;
    travelDir(sats[i], x, y, &ux, &uy);

    const int tip_x = x + static_cast<int>(lroundf(ux * radar::kAircraftNoseLenPx));
    const int tip_y = y + static_cast<int>(lroundf(uy * radar::kAircraftNoseLenPx));
    const int ex = tip_x + static_cast<int>(lroundf(ux * kSatTrackLenPx));
    const int ey = tip_y + static_cast<int>(lroundf(uy * kSatTrackLenPx));
    tft.drawWideLine(tip_x, tip_y, ex, ey, radar::kAircraftTrackLineHalfWidth,
                     radar::kColorTrackVector);
    drawTravelTriangle(x, y, ux, uy, radar::kColorAircraft);
  }

  // Labels for the highest few.
  applyLabelFont();
  tft.setTextColor(radar::kColorLabel, radar::kColorBackground);
  for (size_t li = 0; li < n && li < kMaxLabels; ++li) {
    const services::sat::Satellite& sat = sats[idx[li]];
    int x = 0;
    int y = 0;
    skyToScreen(sat.az_deg, sat.el_deg, &x, &y);
    char name[12];
    strncpy(name, sat.name, sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    const bool on_right = x <= radar::kCenterX;
    const int gap = radar::kAircraftNoseLenPx + radar::kAircraftLabelGapPx + 2;
    tft.setTextDatum(on_right ? textdatum_t::middle_left
                              : textdatum_t::middle_right);
    tft.drawString(name, x + (on_right ? gap : -gap), y);
  }
}

void renderAll() {
  displayFontEnsureLoaded(tft);
  ensureSizes();
  tft.startWrite();
  drawGrid();
  drawSatellites();
  tft.endWrite();
  tft.setTextDatum(textdatum_t::top_left);
}

}  // namespace

void satDisplayDraw() { renderAll(); }

void satDisplayRefresh() { renderAll(); }

void satDisplayShowNoKey() {
  const int cx = radar::kCenterX;
  const int cy = radar::kCenterY;

  displayFontEnsureLoaded(tft);
  ensureSizes();
  tft.startWrite();
  tft.fillScreen(radar::kColorBackground);
  applyLabelFont();
  tft.setTextDatum(textdatum_t::middle_center);
  tft.setTextColor(radar::kColorLabel, radar::kColorBackground);
  tft.drawString("Satellite radar", cx, cy - 20);
  tft.drawString("Set N2YO API key at", cx, cy + 2);
  tft.drawString(config::kPortalHostUrl, cx, cy + 22);
  tft.endWrite();
  tft.setTextDatum(textdatum_t::top_left);
}

}  // namespace ui
