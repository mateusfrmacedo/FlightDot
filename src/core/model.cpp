#include "model.h"
#include <algorithm>
#include <cstring>
constexpr double RAD = 0.017453292519943295;
double distanceKm(double a, double b, double c, double d) {
  double x = sin((c - a) * RAD / 2), y = sin((d - b) * RAD / 2);
  double h = x * x + cos(a * RAD) * cos(c * RAD) * y * y;
  return 12742 * asin(sqrt(std::clamp(h, 0.0, 1.0)));
}
double bearingDeg(double a, double b, double c, double d) {
  double x = sin((d - b) * RAD) * cos(c * RAD),
         y = cos(a * RAD) * sin(c * RAD) - sin(a * RAD) * cos(c * RAD) * cos((d - b) * RAD);
  return fmod(atan2(x, y) / RAD + 360, 360);
}
bool emergency(const Aircraft &a) {
  return !strcmp(a.squawk, "7500") || !strcmp(a.squawk, "7600") || !strcmp(a.squawk, "7700");
}
bool validSettings(const Settings &s) {
  return std::isfinite(s.lat) && std::isfinite(s.lon) && fabs(s.lat) <= 90 && fabs(s.lon) <= 180 &&
         s.rangeKm >= 10 && s.rangeKm <= 250 && s.theme >= 0 && s.theme <= 4 &&
         s.brightness >= 10 && s.brightness <= 255 && s.utcOffsetMinutes >= -840 &&
         s.utcOffsetMinutes <= 840 && s.staleSeconds >= 10 && s.staleSeconds <= 300 &&
         memchr(s.timezone, 0, sizeof(s.timezone)) && memchr(s.centerName, 0, sizeof(s.centerName));
}

bool normalizeFlight(const char *input, char *output, size_t capacity) {
  if (!input || !output || capacity < 2)
    return false;
  size_t count = 0;
  bool letter = false, digit = false;
  for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
    if (*p == ' ' || *p == '\t')
      continue;
    unsigned char c = *p >= 'a' && *p <= 'z' ? *p - 'a' + 'A' : *p;
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) || count + 1 >= capacity) {
      output[0] = 0;
      return false;
    }
    letter |= c >= 'A' && c <= 'Z';
    digit |= c >= '0' && c <= '9';
    output[count++] = c;
  }
  output[count] = 0;
  return count == 0 || (count >= 3 && count <= 8 && letter && digit);
}
