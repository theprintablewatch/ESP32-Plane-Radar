#pragma once

#include <cstddef>

namespace services::sat {

struct Satellite {
  float az_deg;       // azimuth from observer, 0 = north, clockwise
  float el_deg;       // elevation above horizon (0 = horizon, 90 = zenith)
  float prev_az_deg;  // position at the previous fetch (for travel direction)
  float prev_el_deg;
  bool has_prev;      // false until this satellite has been seen twice
  int id;             // NORAD id
  char name[16];      // satellite name (truncated)
};

constexpr size_t kMaxSatellites = 48;

/** Load the saved N2YO API key from NVS. Call once at boot. */
void init();

bool hasApiKey();
const char* apiKey();
/** Persist a new N2YO API key (trimmed); takes effect on the next fetch. */
void saveApiKey(const char* key);

/** Active N2YO category id, and its label (falls back to "SAT"). */
int category();
const char* categoryName();
/** Persist a new category id; takes effect on the next fetch. */
void saveCategory(int category_id);

size_t count();
const Satellite* list();

/** Query N2YO for satellites above observer_lat/lon and fill the list with
 *  their current azimuth/elevation. Returns false on network/parse failure. */
bool fetchUpdate(double observer_lat, double observer_lon);

}  // namespace services::sat
