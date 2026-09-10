#include "parser.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
bool parseAircraft(const std::function<int()> &source, const Settings &s, Snapshot &out, char *err,
                   size_t n) {
  out.count = 0;
  out.valid = false;
  size_t bytes = 0;
  auto get = [&]() { return ++bytes <= 2 * 1024 * 1024 ? source() : -1; };
  auto fail = [&](const char *x) {
    snprintf(err, n, "%s", x);
    out.count = 0;
    return false;
  };
  auto ws = [&]() {
    int c;
    do {
      c = get();
    } while (c >= 0 && isspace(c));
    return c;
  };
  if (ws() != '{')
    return fail("JSON: raiz invalida");
  // Find an aircraft array only at root depth, respecting strings/escapes.
  int depth = 1;
  bool in = false, escape = false;
  char key[32]{};
  size_t k = 0;
  bool target = false;
  while (depth > 0) {
    int c = get();
    if (c < 0)
      return fail("JSON truncado/limite");
    if (in) {
      if (escape) {
        escape = false;
        continue;
      }
      if (c == '\\') {
        escape = true;
        continue;
      }
      if (c == '"') {
        in = false;
        if (depth == 1) {
          key[k] = 0;
          target = !strcmp(key, "ac") || !strcmp(key, "aircraft");
        }
        continue;
      }
      if (depth == 1 && k < sizeof(key) - 1)
        key[k++] = c;
      continue;
    }
    if (c == '"') {
      in = true;
      k = 0;
      continue;
    }
    if (target && c == ':') {
      if (ws() != '[')
        return fail("JSON: aeronaves nao e lista");
      break;
    }
    if (c == '{' || c == '[')
      depth++;
    if (c == '}' || c == ']')
      depth--;
  }
  if (!depth)
    return fail("JSON: lista ausente");
  DynamicJsonDocument doc(4096), filter(512);
  for (auto f : {"hex", "flight", "r", "t", "lat", "lon", "alt_baro", "gs", "track", "true_heading",
                 "baro_rate", "squawk", "seen_pos", "category", "dbFlags"})
    filter[f] = true;
  int c = ws();
  while (c != ']') {
    if (c != '{')
      return fail("JSON: aeronave invalida");
    // One record at a time: memory never grows with aircraft count.
    char object[8192];
    size_t used = 0;
    object[used++] = '{';
    depth = 1;
    in = false;
    escape = false;
    while (depth) {
      c = get();
      if (c < 0)
        return fail("JSON truncado");
      if (used >= sizeof(object) - 1)
        return fail("JSON: registro >8KB");
      object[used++] = c;
      if (in) {
        if (escape)
          escape = false;
        else if (c == '\\')
          escape = true;
        else if (c == '"')
          in = false;
      } else if (c == '"')
        in = true;
      else if (c == '{' || c == '[')
        depth++;
      else if (c == '}' || c == ']')
        depth--;
    }
    object[used] = 0;
    doc.clear();
    auto e = deserializeJson(doc, object, DeserializationOption::Filter(filter),
                             DeserializationOption::NestingLimit(12));
    if (e)
      return fail("JSON: registro malformado");
    if (doc["lat"].is<double>() && doc["lon"].is<double>() && doc["hex"].is<const char *>()) {
      Aircraft a;
      a.lat = doc["lat"];
      a.lon = doc["lon"];
      a.seen = doc["seen_pos"] | 0.f;
      a.distance = distanceKm(s.lat, s.lon, a.lat, a.lon);
      a.bearing = bearingDeg(s.lat, s.lon, a.lat, a.lon);
      if (std::isfinite(a.lat) && std::isfinite(a.lon) && fabs(a.lat) <= 90 && fabs(a.lon) <= 180 &&
          a.seen <= 60 && a.distance <= s.rangeKm) {
        snprintf(a.hex, sizeof(a.hex), "%s", doc["hex"] | "");
        snprintf(a.callsign, sizeof(a.callsign), "%s", doc["flight"] | "");
        size_t l = strlen(a.callsign);
        while (l && isspace((unsigned char)a.callsign[l - 1]))
          a.callsign[--l] = 0;
        snprintf(a.registration, sizeof(a.registration), "%s", doc["r"] | "");
        snprintf(a.type, sizeof(a.type), "%s", doc["t"] | "");
        snprintf(a.category, sizeof(a.category), "%s", doc["category"] | "");
        a.military = (int(doc["dbFlags"] | 0) & 1) != 0;
        if (doc["squawk"].is<const char *>())
          snprintf(a.squawk, sizeof(a.squawk), "%s", doc["squawk"].as<const char *>());
        else if (doc["squawk"].is<int>())
          snprintf(a.squawk, sizeof(a.squawk), "%04d", doc["squawk"].as<int>());
        a.ground = doc["alt_baro"].is<const char *>() && !strcmp(doc["alt_baro"], "ground");
        a.altitude = a.ground ? 0 : (doc["alt_baro"] | NAN);
        a.speed = doc["gs"] | NAN;
        a.heading = doc["track"] | (doc["true_heading"] | NAN);
        a.verticalRate = doc["baro_rate"] | NAN;
        size_t idx = out.count;
        for (size_t j = 0; j < out.count; j++)
          if (!strcmp(a.hex, out.planes[j].hex)) {
            idx = j;
            break;
          }
        if (idx == MAX_AIRCRAFT) {
          idx = std::max_element(
                    out.planes, out.planes + out.count,
                    [](const Aircraft &x, const Aircraft &y) { return x.distance < y.distance; }) -
                out.planes;
          if (a.distance >= out.planes[idx].distance)
            idx = MAX_AIRCRAFT;
        }
        if (idx < MAX_AIRCRAFT) {
          out.planes[idx] = a;
          if (idx == out.count)
            out.count++;
        }
      }
    }
    c = ws();
    if (c == ',') {
      c = ws();
      if (c == ']')
        return fail("JSON: virgula final");
    } else if (c != ']')
      return fail("JSON: separador invalido");
  }
  // Validate the remaining root envelope (bounded), including metadata after array.
  char tail[2048];
  size_t len = 0;
  tail[len++] = '{';
  tail[len++] = '"';
  tail[len++] = 'x';
  tail[len++] = '"';
  tail[len++] = ':';
  tail[len++] = '0';
  while ((c = get()) >= 0) {
    if (len >= sizeof(tail) - 1)
      return fail("JSON: envelope grande");
    tail[len++] = c;
  }
  tail[len] = 0;
  doc.clear();
  if (deserializeJson(doc, tail))
    return fail("JSON: envelope truncado");
  std::sort(out.planes, out.planes + out.count,
            [](const Aircraft &a, const Aircraft &b) { return a.distance < b.distance; });
  out.valid = true;
  err[0] = 0;
  return true;
}
