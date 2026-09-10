#include "places.h"
#include "places_generated.h"
#include <algorithm>
#include <cmath>

size_t nearbyPlaces(double lat, double lon, float rangeKm, VisiblePlace *out, size_t capacity) {
  if (!out || !capacity)
    return 0;
  size_t count = 0;
  const double latWindow = rangeKm / 110.5 + .08;
  const double lonScale = std::max(.15, fabs(cos(lat * .01745329252)));
  const double lonWindow = rangeKm / (111.3 * lonScale) + .08;
  for (unsigned i = 0; i < PLACE_RECORD_COUNT; ++i) {
    const auto &p = PLACE_RECORDS[i];
    double plat = p.latE5 / 100000.0, plon = p.lonE5 / 100000.0;
    if (fabs(plat - lat) > latWindow || fabs(plon - lon) > lonWindow)
      continue;
    float distance = distanceKm(lat, lon, plat, plon);
    if (distance > rangeKm || distance < .4f)
      continue;
    VisiblePlace value{plat,   plon,   distance, float(bearingDeg(lat, lon, plat, plon)),
                       p.name, p.code, p.kind};
    if (count < capacity)
      out[count++] = value;
    else {
      auto farthest =
          std::max_element(out, out + count, [](const VisiblePlace &a, const VisiblePlace &b) {
            // Retain airports slightly farther away so at least regional fields appear.
            float aa = a.distance * (a.kind == 'A' ? .72f : 1.f);
            float bb = b.distance * (b.kind == 'A' ? .72f : 1.f);
            return aa < bb;
          });
      float worst = farthest->distance * (farthest->kind == 'A' ? .72f : 1.f);
      float score = distance * (p.kind == 'A' ? .72f : 1.f);
      if (score < worst)
        *farthest = value;
    }
  }
  std::sort(out, out + count,
            [](const VisiblePlace &a, const VisiblePlace &b) { return a.distance < b.distance; });
  return count;
}
