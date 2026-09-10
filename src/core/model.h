#pragma once
#include <cmath>
#include <stddef.h>
#include <stdint.h>
constexpr size_t MAX_AIRCRAFT = 80;
struct Settings {
  double lat = -20.183608, lon = -49.354661;
  int rangeKm = 150, theme = 0, brightness = 150, utcOffsetMinutes = -180, staleSeconds = 20;
  bool sweep = true, autoDim = false;
  char timezone[64] = "<-03>3";
  char centerName[36] = "Orindiúva";
};
struct Aircraft {
  char hex[12]{}, callsign[20]{}, registration[20]{}, type[20]{}, squawk[8]{}, category[4]{};
  double lat = 0, lon = 0;
  float altitude = NAN, speed = NAN, heading = NAN, verticalRate = NAN, distance = 0, bearing = 0,
        seen = 0;
  bool ground = false, military = false;
};
struct Snapshot {
  Aircraft planes[MAX_AIRCRAFT];
  size_t count = 0;
  uint32_t updatedMs = 0, generation = 0;
  bool valid = false, mock = false, following = false;
  double centerLat = 0, centerLon = 0;
  char provider[24]{};
};
inline void clearSnapshot(Snapshot &s) {
  s.count = 0;
  s.updatedMs = 0;
  s.generation = 0;
  s.valid = false;
  s.mock = false;
  s.following = false;
  s.centerLat = s.centerLon = 0;
  s.provider[0] = 0;
}
struct Route {
  char hex[12]{}, callsign[20]{}, origin[12]{}, destination[12]{}, type[64]{};
  char airline[48]{}, destinationName[56]{}, destinationCity[40]{};
  double originLat = NAN, originLon = NAN, destinationLat = NAN, destinationLon = NAN;
  float temperature = NAN;
  uint32_t fetchedAt = 0;
  bool found = false;
};
struct Status {
  bool wifi = false, setup = false;
  char ip[24]{};
  char clock[20] = "--:--";
  char date[20]{};
  char error[80]{};
  char follow[20]{};
  float voltage = 0;
  float fps = 0;
};
double distanceKm(double a, double b, double c, double d);
double bearingDeg(double a, double b, double c, double d);
bool emergency(const Aircraft &a);
bool validSettings(const Settings &s);
void mockSnapshot(Snapshot &out, const Settings &s, uint32_t now);

// Normalize a flight identifier; an empty string means stop following.
bool normalizeFlight(const char *input, char *output, size_t capacity);
