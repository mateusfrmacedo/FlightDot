#include "rtc.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>
namespace rtc {
static int dec(uint8_t b) {
  return (b >> 4) * 10 + (b & 15);
}
static uint8_t bcd(int v) {
  return (v / 10) * 16 + v % 10;
}
void begin() {
  Wire.beginTransmission(0x51);
  Wire.write(4);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(0x51), uint8_t(7)) != 7)
    return;
  uint8_t b[7];
  for (auto &x : b)
    x = Wire.read();
  if (b[0] & 0x80)
    return;
  tm t = {};
  t.tm_sec = dec(b[0] & 127);
  t.tm_min = dec(b[1] & 127);
  t.tm_hour = dec(b[2] & 63);
  t.tm_mday = dec(b[3] & 63);
  t.tm_mon = dec(b[5] & 31) - 1;
  t.tm_year = dec(b[6]) + 100;
  if (t.tm_year < 124 || t.tm_mon < 0 || t.tm_mon > 11 || t.tm_mday < 1 || t.tm_mday > 31)
    return;
  setenv("TZ", "UTC0", 1);
  tzset();
  timeval tv = {mktime(&t), 0};
  settimeofday(&tv, nullptr);
  Serial.println("[rtc] UTC restaurado do PCF85063");
}
void sync() {
  time_t now = time(nullptr);
  if (now < 1700000000)
    return;
  tm t;
  gmtime_r(&now, &t);
  uint8_t b[] = {bcd(t.tm_sec),  bcd(t.tm_min),     bcd(t.tm_hour),      bcd(t.tm_mday),
                 bcd(t.tm_wday), bcd(t.tm_mon + 1), bcd(t.tm_year - 100)};
  Wire.beginTransmission(0x51);
  Wire.write(4);
  Wire.write(b, 7);
  Wire.endTransmission();
}
float voltage() {
  return analogReadMilliVolts(pins::voltage) * .003f;
} // Official schematic: 200k / 100k divider, system VIN, NOT battery state-of-charge.
} // namespace rtc
