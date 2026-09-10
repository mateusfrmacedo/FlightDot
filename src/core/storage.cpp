#include "storage.h"
#include <Preferences.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <time.h>
namespace storage {
struct LegacySettingsV1 {
  double lat, lon;
  int rangeKm, theme, brightness, utcOffsetMinutes, staleSeconds;
  bool sweep, autoDim;
  char timezone[64];
};
struct LegacySettingsV2 {
  double lat, lon;
  int rangeKm, theme, brightness, utcOffsetMinutes, staleSeconds;
  bool sweep, autoDim;
  char timezone[64];
  char centerName[36];
};
static_assert(sizeof(LegacySettingsV2) == sizeof(Settings), "settings v2 layout changed");
Settings load() {
  Settings s;
  Preferences p;
  p.begin("plano", true);
  unsigned version = p.getUInt("version");
  if (version == 3 && p.getBytesLength("settings") == sizeof(s))
    p.getBytes("settings", &s, sizeof(s));
  else if (version == 2 && p.getBytesLength("settings") == sizeof(LegacySettingsV2)) {
    LegacySettingsV2 old{};
    p.getBytes("settings", &old, sizeof(old));
    memcpy(&s, &old, sizeof(old));
    // Old index 2 was the removed dotted theme.
    s.theme = old.theme == 2 ? 0 : old.theme > 2 ? old.theme - 1 : old.theme;
  }
  else if (version == 1 && p.getBytesLength("settings") == sizeof(LegacySettingsV1)) {
    LegacySettingsV1 old{};
    p.getBytes("settings", &old, sizeof(old));
    s.lat = old.lat;
    s.lon = old.lon;
    s.rangeKm = old.rangeKm;
    s.theme = std::min(old.theme, 1);
    s.brightness = old.brightness;
    s.utcOffsetMinutes = old.utcOffsetMinutes;
    s.staleSeconds = old.staleSeconds;
    s.sweep = old.sweep;
    snprintf(s.timezone, sizeof(s.timezone), "%s", old.timezone);
    snprintf(s.centerName, sizeof(s.centerName), "Orindiúva");
  }
  p.end();
  if (!strcmp(s.centerName, "Centro salvo") || !strcmp(s.centerName, "Orindiuva"))
    snprintf(s.centerName, sizeof(s.centerName), "Orindiúva");
  s.autoDim = false; // Retain the stored layout while retiring automatic dimming.
  return validSettings(s) ? s : Settings{};
}
bool save(const Settings &s) {
  if (!validSettings(s))
    return false;
  Preferences p;
  if (!p.begin("plano", false))
    return false;
  bool ok = p.putBytes("settings", &s, sizeof(s)) == sizeof(s);
  if (ok)
    p.putUInt("version", 3);
  p.end();
  return ok;
}
static unsigned slot(const char *a, const char *b) {
  unsigned h = 2166136261u;
  for (; *a; a++)
    h = (h ^ *a) * 16777619;
  for (; *b; b++)
    h = (h ^ *b) * 16777619;
  return h % 24;
}
bool readRoute(const char *hex, const char *call, Route &r) {
  char key[12];
  snprintf(key, sizeof(key), "r%02u", slot(hex, call));
  Preferences p;
  p.begin("routes", true);
  bool ok = p.getBytesLength(key) == sizeof(r) && p.getBytes(key, &r, sizeof(r)) == sizeof(r);
  p.end();
  time_t now = time(nullptr);
  return ok && !strcmp(r.hex, hex) && !strcmp(r.callsign, call) && now >= r.fetchedAt &&
         now - r.fetchedAt < (r.found ? 21600 : 900);
}
void writeRoute(const Route &r) {
  if (r.fetchedAt < 1700000000)
    return;
  char key[12];
  snprintf(key, sizeof(key), "r%02u", slot(r.hex, r.callsign));
  Preferences p;
  if (p.begin("routes", false)) {
    p.putBytes(key, &r, sizeof(r));
    p.end();
  }
}
} // namespace storage
