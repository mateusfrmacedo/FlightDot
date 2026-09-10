#include "model.h"
#include <cstdio>
#include <cstring>
void mockSnapshot(Snapshot &out, const Settings &s, uint32_t now) {
  clearSnapshot(out);
  out.count = 8;
  out.valid = true;
  out.mock = true;
  out.updatedMs = now;
  strcpy(out.provider, "DEMONSTRACAO");
  for (size_t i = 0; i < out.count; i++) {
    auto &a = out.planes[i];
    a = Aircraft{};
    double t = i * .79 + now * .000008;
    a.lat = s.lat + cos(t) * (.12 + i * .055);
    a.lon = s.lon + sin(t) * (.12 + i * .065);
    snprintf(a.hex, sizeof(a.hex), "DE%04u", unsigned(i));
    snprintf(a.callsign, sizeof(a.callsign), "DEMO%03u", unsigned(i + 1));
    strcpy(a.type, i % 2 ? "A320" : "B738");
    if (i == 2)
      strcpy(a.category, "A7");
    else if (i == 4)
      strcpy(a.category, "B2");
    else if (i == 6)
      strcpy(a.category, "B6");
    strcpy(a.squawk, i == 7 ? "7700" : "2000");
    a.heading = fmod(t * 180 / 3.14159265 + 90, 360);
    a.altitude = 2000 + i * 5000;
    a.speed = 180 + i * 35;
    a.distance = distanceKm(s.lat, s.lon, a.lat, a.lon);
    a.bearing = bearingDeg(s.lat, s.lon, a.lat, a.lon);
  }
}
