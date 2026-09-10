#include "touch.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
namespace touch {
void begin() {
  Wire.begin(pins::sda, pins::scl, 300000);
  Wire.setTimeOut(20);
  Wire.beginTransmission(0x38);
  Wire.write(0);
  Wire.write(0);
  Serial.printf("[touch] FT3168 protocol @0x38: %s\n",
                Wire.endTransmission() == 0 ? "ACK" : "NOT FOUND");
  for (uint8_t reg : {uint8_t(0xA3), uint8_t(0xA8)}) {
    Wire.beginTransmission(0x38);
    Wire.write(reg);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(uint8_t(0x38), uint8_t(1)) == 1)
      Serial.printf("[touch] reg 0x%02x = 0x%02x\n", reg, Wire.read());
  }
}
void read(lv_indev_drv_t *, lv_indev_data_t *d) {
  d->state = LV_INDEV_STATE_RELEASED;
  Wire.beginTransmission(0x38);
  Wire.write(2);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(uint8_t(0x38), uint8_t(5)) != 5)
    return;
  uint8_t b[5];
  for (auto &v : b)
    v = Wire.read();
  if ((b[0] & 15) == 0 || (b[0] & 15) > 2 || (b[1] >> 6) == 1)
    return;
  int x = ((b[1] & 15) << 8) | b[2], y = ((b[3] & 15) << 8) | b[4];
  if (x >= 466 || y >= 466)
    return;
  // Match the 270-degree display transform implemented in display::flush.
  d->point.x = y;
  d->point.y = 465 - x;
  d->state = LV_INDEV_STATE_PRESSED;
}
} // namespace touch
