#include "services/web_settings.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include "config.h"
#include "services/radar_location.h"
#include "services/radar_rotation.h"
#include "services/sat_client.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"

namespace services::web_settings {

namespace {
WebServer s_server(80);

/** Strip spaces and upper-case a UK postcode in place for the API path. */
String normalizePostcode(const String& raw) {
  String out;
  for (size_t i = 0; i < raw.length(); ++i) {
    const char c = raw[i];
    if (c != ' ') {
      out += static_cast<char>(toupper(c));
    }
  }
  return out;
}

/** Look up a UK postcode via postcodes.io. Returns true and fills lat/lon. */
bool geocodePostcode(const String& postcode, double* out_lat, double* out_lon) {
  const String pc = normalizePostcode(postcode);
  if (pc.length() == 0) {
    return false;
  }

  String url = "https://api.postcodes.io/postcodes/";
  url += pc;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("postcode: http.begin failed");
    return false;
  }

  http.setTimeout(10000);
  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("postcode: HTTP %d\n", code);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("postcode: JSON parse error: %s\n", err.c_str());
    return false;
  }

  JsonObject result = doc["result"].as<JsonObject>();
  if (result.isNull() || !result["latitude"].is<double>() ||
      !result["longitude"].is<double>()) {
    Serial.println("postcode: missing coordinates in response");
    return false;
  }

  *out_lat = result["latitude"].as<double>();
  *out_lon = result["longitude"].as<double>();
  Serial.printf("postcode: %s -> %.6f, %.6f\n", pc.c_str(), *out_lat, *out_lon);
  return true;
}

void sendPostcodeError(const String& postcode) {
  String html =
      "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"
      "<body style=\"font-family: sans-serif; padding: 20px;\">"
      "<h2>Postcode lookup failed</h2>"
      "<p>Could not find coordinates for <b>" + postcode + "</b>.</p>"
      "<p>Check the postcode (UK only) or enter latitude/longitude manually.</p>"
      "<p><a href=\"/\">Back to settings</a></p>"
      "</body></html>";
  s_server.send(404, "text/html", html);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"
                "<body style=\"font-family: sans-serif; padding: 20px;\">"
                "<h2>Radar Settings</h2>"
                "<form method=\"POST\" action=\"/save\">"
                "<label>UK Postcode (optional &mdash; fills coordinates below):</label><br>"
                "<input type=\"text\" name=\"postcode\" placeholder=\"e.g. SW1A 1AA\"><br><br>"
                "<label>Latitude:</label><br>"
                "<input type=\"number\" step=\"0.000001\" name=\"lat\" value=\"" + String(services::location::lat(), 6) + "\"><br><br>"
                "<label>Longitude:</label><br>"
                "<input type=\"number\" step=\"0.000001\" name=\"lon\" value=\"" + String(services::location::lon(), 6) + "\"><br><br>"
                "<label><input type=\"checkbox\" name=\"miles\" value=\"T\"" + String(ui::radar::useMiles() ? " checked" : "") + "> Display distances in miles</label><br><br>"
                "<label>Top Bearing (degrees) Hint use your phone compass:</label><br>"
                "<input type=\"number\" step=\"1\" name=\"top_bearing\" value=\"" + String(services::radar_rotation::topHeading(), 0) + "\"><br><br>"
                "<label>N2YO API Key (satellite radar &mdash; swipe to switch):</label><br>"
                "<input type=\"text\" name=\"n2yo_key\" placeholder=\"from n2yo.com/api\" value=\"" + String(services::sat::apiKey()) + "\"><br><br>"
                "<label>Satellite category:</label><br>"
                "<select name=\"sat_cat\">";

  for (size_t i = 0; i < config::kSatCategoryCount; ++i) {
    const config::SatCategory& cat = config::kSatCategories[i];
    const bool selected = (cat.id == services::sat::category());
    html += "<option value=\"" + String(cat.id) + "\"" +
            (selected ? " selected" : "") + ">" + String(cat.name) + "</option>";
  }

  html +=       "</select><br><br>"
                "<label>Range:</label><br>"
                "<select name=\"range\">";

  for (size_t i = 0; i < ui::radar::kRangePresetCount; ++i) {
    char label[16];
    ui::radar::formatRing3Label(label, sizeof(label), ui::radar::kRangePresets[i].ring3_km, ui::radar::useMiles());
    bool is_current = (ui::radar::rangeCurrent().outer_km == ui::radar::kRangePresets[i].outer_km);
    html += "<option value=\"" + String(i) + "\"" + (is_current ? " selected" : "") + ">" + String(label) + "</option>";
  }

  html +=       "</select><br><br>"
                "<input type=\"submit\" value=\"Save Settings\">"
                "</form></body></html>";
  s_server.send(200, "text/html", html);
}

void handleSave() {
  String lat = s_server.arg("lat");
  String lon = s_server.arg("lon");
  String miles = s_server.arg("miles");
  String range_idx_str = s_server.arg("range");
  String top_bearing_str = s_server.arg("top_bearing");
  String postcode = s_server.arg("postcode");
  postcode.trim();

  // A postcode takes priority: geocode it and override the lat/lon fields.
  if (postcode.length() > 0) {
    double glat = 0.0;
    double glon = 0.0;
    if (!geocodePostcode(postcode, &glat, &glon)) {
      sendPostcodeError(postcode);
      return;
    }
    lat = String(glat, 6);
    lon = String(glon, 6);
  }

  services::location::saveFromStrings(lat.c_str(), lon.c_str());
  ui::radar::saveMilesFromPortal(miles == "T" ? "T" : "F");

  String n2yo_key = s_server.arg("n2yo_key");
  n2yo_key.trim();
  services::sat::saveApiKey(n2yo_key.c_str());

  String sat_cat = s_server.arg("sat_cat");
  if (sat_cat.length() > 0) {
    services::sat::saveCategory(sat_cat.toInt());
  }

  if (top_bearing_str.length() > 0) {
    services::radar_rotation::setTopHeading(top_bearing_str.toFloat());
  }

  if (range_idx_str.length() > 0) {
    int target_index = range_idx_str.toInt();
    if (target_index >= 0 && target_index < static_cast<int>(ui::radar::kRangePresetCount)) {
      int current_idx = 0;
      // Find current range index
      for (size_t i = 0; i < ui::radar::kRangePresetCount; ++i) {
        if (ui::radar::rangeCurrent().outer_km == ui::radar::kRangePresets[i].outer_km) {
          current_idx = i;
          break;
        }
      }
      // Cycle through ranges until we hit the user's selected option
      while (current_idx != target_index) {
        ui::radar::rangeNext();
        current_idx = (current_idx + 1) % ui::radar::kRangePresetCount;
      }
    }
  }

  // Force a redraw so the screen immediately uses the new location/units
  ui::radarDisplayRefreshRange();

  s_server.sendHeader("Location", "/");
  s_server.send(303);
}
}  // namespace

void init() {
  if (MDNS.begin(config::kPortalHostname)) {
    MDNS.addService("http", "tcp", 80);
  }
  s_server.on("/", HTTP_GET, handleRoot);
  s_server.on("/save", HTTP_POST, handleSave);
  s_server.begin();
}

void handle() {
  s_server.handleClient();
}

}  // namespace services::web_settings