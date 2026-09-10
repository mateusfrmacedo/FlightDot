#pragma once
#include "model.h"

struct VisiblePlace {
  double lat, lon;
  float distance, bearing;
  const char *name;
  const char *code;
  char kind;
};

// Rebuild a bounded nearest-landmark set after the radar center changes.
size_t nearbyPlaces(double lat, double lon, float rangeKm, VisiblePlace *out, size_t capacity);

