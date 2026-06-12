#include "services/sat_client.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <cmath>
#include <cstring>

#include "config.h"

namespace services::sat {

namespace {

constexpr char kPrefsNamespace[] = "sat";
constexpr char kKeyApi[] = "apikey";
constexpr char kKeyCategory[] = "category";
constexpr char kApiBase[] = "https://api.n2yo.com/rest/v1/satellite/above/";

constexpr double kEarthRadiusKm = 6371.0;
constexpr double kDegToRad = 0.017453292519943295;
constexpr double kRadToDeg = 57.29577951308232;

Satellite s_sats[kMaxSatellites];
size_t s_count = 0;
String s_key = config::kN2yoDefaultApiKey;
int s_category = config::kSatCategory;

// Snapshot of the previous fetch, used to derive each satellite's direction of
// travel (N2YO only reports position, not heading).
struct PrevPos {
  int id;
  float az_deg;
  float el_deg;
};
PrevPos s_prev[kMaxSatellites];
size_t s_prev_count = 0;

bool lookupPrev(int id, float* az, float* el) {
  for (size_t i = 0; i < s_prev_count; ++i) {
    if (s_prev[i].id == id) {
      *az = s_prev[i].az_deg;
      *el = s_prev[i].el_deg;
      return true;
    }
  }
  return false;
}

/** Azimuth/elevation of a satellite (geodetic lat/lon/alt) as seen from an
 *  observer at sea level. Spherical Earth — plenty accurate for the display. */
void lookAngles(double obs_lat, double obs_lon, double sat_lat, double sat_lon,
                double sat_alt_km, float* az, float* el) {
  const double phi = obs_lat * kDegToRad;
  const double lam = obs_lon * kDegToRad;
  const double c_phi = cos(phi);
  const double s_phi = sin(phi);
  const double c_lam = cos(lam);
  const double s_lam = sin(lam);

  const double ox = kEarthRadiusKm * c_phi * c_lam;
  const double oy = kEarthRadiusKm * c_phi * s_lam;
  const double oz = kEarthRadiusKm * s_phi;

  const double p2 = sat_lat * kDegToRad;
  const double l2 = sat_lon * kDegToRad;
  const double rs = kEarthRadiusKm + sat_alt_km;
  const double sx = rs * cos(p2) * cos(l2);
  const double sy = rs * cos(p2) * sin(l2);
  const double sz = rs * sin(p2);

  const double dx = sx - ox;
  const double dy = sy - oy;
  const double dz = sz - oz;

  const double east = -s_lam * dx + c_lam * dy;
  const double north = -s_phi * c_lam * dx - s_phi * s_lam * dy + c_phi * dz;
  const double up = c_phi * c_lam * dx + c_phi * s_lam * dy + s_phi * dz;

  const double horiz = sqrt(east * east + north * north);
  double a = atan2(east, north) * kRadToDeg;
  if (a < 0.0) {
    a += 360.0;
  }
  *az = static_cast<float>(a);
  *el = static_cast<float>(atan2(up, horiz) * kRadToDeg);
}

}  // namespace

void init() {
  Preferences prefs;
  prefs.begin(kPrefsNamespace, true);
  if (prefs.isKey(kKeyApi)) {
    s_key = prefs.getString(kKeyApi, config::kN2yoDefaultApiKey);
  }
  if (prefs.isKey(kKeyCategory)) {
    s_category = prefs.getInt(kKeyCategory, config::kSatCategory);
  }
  prefs.end();
}

bool hasApiKey() { return s_key.length() > 0; }

const char* apiKey() { return s_key.c_str(); }

void saveApiKey(const char* key) {
  s_key = key != nullptr ? key : "";
  s_key.trim();
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putString(kKeyApi, s_key);
  prefs.end();
  Serial.printf("sat: N2YO key %s\n", hasApiKey() ? "saved" : "cleared");
}

int category() { return s_category; }

const char* categoryName() {
  for (size_t i = 0; i < config::kSatCategoryCount; ++i) {
    if (config::kSatCategories[i].id == s_category) {
      return config::kSatCategories[i].name;
    }
  }
  return "SAT";
}

void saveCategory(int category_id) {
  s_category = category_id;
  Preferences prefs;
  prefs.begin(kPrefsNamespace, false);
  prefs.putInt(kKeyCategory, s_category);
  prefs.end();
  Serial.printf("sat: category %d (%s)\n", s_category, categoryName());
}

size_t count() { return s_count; }

const Satellite* list() { return s_sats; }

bool fetchUpdate(double observer_lat, double observer_lon) {
  if (!hasApiKey()) {
    return false;
  }

  String url = kApiBase;
  url += String(observer_lat, 6);
  url += "/";
  url += String(observer_lon, 6);
  url += "/0/";  // observer altitude (m)
  url += String(config::kSatSearchRadiusDeg);
  url += "/";
  url += String(s_category);
  url += "/&apiKey=";
  url += s_key;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("sat: http.begin failed");
    return false;
  }

  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("sat: HTTP %d\n", code);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();
  Serial.printf("sat: payload %u bytes\n",
                static_cast<unsigned>(payload.length()));

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("sat: JSON parse error: %s\n", err.c_str());
    return false;
  }

  // N2YO reports errors in the "error" field and a count in info.satcount.
  if (doc["error"].is<const char*>()) {
    Serial.printf("sat: API error: %s\n", doc["error"].as<const char*>());
    return false;
  }
  Serial.printf("sat: info.satcount=%d\n", doc["info"]["satcount"].as<int>());

  JsonArray above = doc["above"].as<JsonArray>();
  if (above.isNull()) {
    s_count = 0;
    return true;
  }

  // Snapshot the current list as "previous" before refilling, so each
  // satellite can carry the position it had at the last fetch.
  s_prev_count = s_count;
  for (size_t i = 0; i < s_count; ++i) {
    s_prev[i].id = s_sats[i].id;
    s_prev[i].az_deg = s_sats[i].az_deg;
    s_prev[i].el_deg = s_sats[i].el_deg;
  }

  size_t n = 0;
  for (JsonObject sat : above) {
    if (n >= kMaxSatellites) {
      break;
    }
    if (sat["satlat"].isNull() || sat["satlng"].isNull()) {
      continue;
    }

    const double slat = sat["satlat"].as<double>();
    const double slng = sat["satlng"].as<double>();
    const double salt = sat["satalt"].as<double>();

    float az = 0.0f;
    float el = 0.0f;
    lookAngles(observer_lat, observer_lon, slat, slng, salt, &az, &el);
    if (el < config::kSatMinElevationDeg) {
      continue;
    }

    s_sats[n].az_deg = az;
    s_sats[n].el_deg = el;
    s_sats[n].id = sat["satid"].as<int>();
    s_sats[n].has_prev =
        lookupPrev(s_sats[n].id, &s_sats[n].prev_az_deg, &s_sats[n].prev_el_deg);
    const char* name =
        sat["satname"].is<const char*>() ? sat["satname"].as<const char*>() : "";
    strncpy(s_sats[n].name, name, sizeof(s_sats[n].name) - 1);
    s_sats[n].name[sizeof(s_sats[n].name) - 1] = '\0';
    ++n;
  }

  s_count = n;
  Serial.printf("sat: %u satellites above\n", static_cast<unsigned>(n));
  return true;
}

}  // namespace services::sat
