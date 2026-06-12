#include "ui/detection_list.h"

#if defined(BOARD_WAVESHARE_43B)

#include <lgfx/v1/lgfx_fonts.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "config.h"
#include "hardware/display.h"
#include "hardware/display_font.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "ui/radar_range.h"
#include "ui/radar_theme.h"

namespace fonts = lgfx::v1::fonts;

namespace ui {
namespace {

// Panel occupies the column to the right of the radar circle.
constexpr int kPanelLeft = radar::kCenterX + radar::kGridOuterRadius + 30;
constexpr int kPanelRight = config::kDisplayWidth - 10;
constexpr int kPanelTop = 14;
constexpr int kHeaderH = 46;
constexpr int kRowTop = kPanelTop + kHeaderH + 6;
constexpr int kRowGap = 10;
constexpr int kPanelBottom = config::kDisplayHeight - 10;

// VLW scale factors. The 4.3B master is ~28px, so these are <=1.0 (downscaling
// a larger master stays crisp; the old values upscaled a 15px master).
constexpr float kTitleVlw = 1.05f;
constexpr float kCallVlw = 0.82f;
constexpr float kSubVlw = 0.66f;

const lgfx::GFXfont* const kTitleGfx = &fonts::FreeSansBold18pt7b;
const lgfx::GFXfont* const kCallGfx = &fonts::FreeSansBold12pt7b;
const lgfx::GFXfont* const kSubGfx = &fonts::FreeSans9pt7b;

constexpr float kKmPerDeg = 111.0f;

float distanceKm(const services::adsb::Aircraft& a) {
  const float dx = static_cast<float>(a.lon - services::location::lon()) * kKmPerDeg;
  const float dy = static_cast<float>(a.lat - services::location::lat()) * kKmPerDeg;
  return std::sqrt(dx * dx + dy * dy);
}

void applyTitleFont() {
  if (displayFontIsSmooth()) displayFontSetSmoothSize(tft, kTitleVlw);
  else displayFontSetBitmap(tft, kTitleGfx);
}
void applyCallFont() {
  if (displayFontIsSmooth()) displayFontSetSmoothSize(tft, kCallVlw);
  else displayFontSetBitmap(tft, kCallGfx);
}
void applySubFont() {
  if (displayFontIsSmooth()) displayFontSetSmoothSize(tft, kSubVlw);
  else displayFontSetBitmap(tft, kSubGfx);
}

void formatDistance(char* buf, size_t len, float km) {
  if (radar::useMiles()) {
    const float mi = km * 0.621371f;
    snprintf(buf, len, mi < 10.0f ? "%.1f mi" : "%.0f mi", mi);
  } else {
    snprintf(buf, len, km < 10.0f ? "%.1f km" : "%.0f km", km);
  }
}

// Indices of current aircraft sorted nearest-first.
size_t collectSorted(size_t* order) {
  const size_t n = services::adsb::aircraftCount();
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();
  for (size_t i = 0; i < n; ++i) order[i] = i;
  for (size_t i = 1; i < n; ++i) {
    const size_t key = order[i];
    const float kd = distanceKm(planes[key]);
    size_t j = i;
    while (j > 0 && distanceKm(planes[order[j - 1]]) > kd) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = key;
  }
  return n;
}

}  // namespace

void detectionListDrawStatic(lgfx::LovyanGFX& gfx) {
  displayFontEnsureLoaded(gfx);
  // Divider between radar and panel.
  gfx.drawFastVLine(kPanelLeft - 16, kPanelTop, kPanelBottom - kPanelTop,
                    radar::kColorGrid);

  if (displayFontIsSmooth()) displayFontSetSmoothSize(gfx, kTitleVlw);
  else displayFontSetBitmap(gfx, kTitleGfx);
  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setTextColor(radar::kColorLabel, radar::kColorBackground);
  gfx.drawString("TRAFFIC", kPanelLeft, kPanelTop);
  gfx.drawFastHLine(kPanelLeft, kPanelTop + kHeaderH - 4, kPanelRight - kPanelLeft,
                    radar::kColorGrid);
  gfx.setTextDatum(textdatum_t::top_left);
}

void detectionListDraw() {
  const services::adsb::Aircraft* planes = services::adsb::aircraftList();
  size_t order[services::adsb::kMaxAircraft];
  const size_t n = collectSorted(order);

  applyCallFont();
  const int call_h = tft.fontHeight();
  applySubFont();
  const int sub_h = tft.fontHeight();
  const int row_h = call_h + sub_h + kRowGap;

  int y = kRowTop;
  for (size_t k = 0; k < n; ++k) {
    if (y + row_h > kPanelBottom) break;  // panel full
    const services::adsb::Aircraft& a = planes[order[k]];

    char dist_buf[12];
    formatDistance(dist_buf, sizeof(dist_buf), distanceKm(a));

    // Line 1: callsign (left, white) + distance (right, grey).
    applyCallFont();
    tft.setTextColor(radar::kColorLabel, radar::kColorBackground);
    tft.setTextDatum(textdatum_t::top_left);
    tft.drawString(a.callsign[0] ? a.callsign : "—", kPanelLeft, y);
    tft.setTextDatum(textdatum_t::top_right);
    tft.setTextColor(radar::kColorGrid, radar::kColorBackground);
    tft.drawString(dist_buf, kPanelRight, y);

    // Line 2: type (amber) + altitude (blue).
    const int y2 = y + call_h;
    applySubFont();
    tft.setTextDatum(textdatum_t::top_left);
    int x = kPanelLeft;
    if (a.type[0]) {
      tft.setTextColor(radar::kColorTagType, radar::kColorBackground);
      tft.drawString(a.type, x, y2);
      x += tft.textWidth(a.type) + 12;
    }
    if (a.alt[0]) {
      tft.setTextColor(radar::kColorTagAltitude, radar::kColorBackground);
      tft.drawString(a.alt, x, y2);
    }

    y += row_h;
  }

  tft.setTextDatum(textdatum_t::top_left);
}

}  // namespace ui

#endif
