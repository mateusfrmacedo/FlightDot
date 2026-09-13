#include "touch.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
namespace touch {
static lv_point_t lastPoint{233, 233}, rawPoint{233, 233};
static bool rawDown = false, stableDown = false;
static uint32_t rawChangedAt = 0;

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
  d->point = lastPoint;
  d->state = LV_INDEV_STATE_RELEASED;
  Wire.beginTransmission(0x38);
  Wire.write(2);
  bool down = Wire.endTransmission(false) == 0 && Wire.requestFrom(uint8_t(0x38), uint8_t(5)) == 5;
  uint8_t b[5];
  if (down) {
    for (auto &v : b)
      v = Wire.read();
    int x = ((b[1] & 15) << 8) | b[2], y = ((b[3] & 15) << 8) | b[4];
    down = (b[0] & 15) >= 1 && (b[0] & 15) <= 2 && (b[1] >> 6) != 1 && x < 466 && y < 466;
    if (down) {
      // Match the 270-degree display transform implemented in display::flush.
      rawPoint.x = y;
      rawPoint.y = 465 - x;
    }
  }
  uint32_t now = millis();
  if (down != rawDown) {
    rawDown = down;
    rawChangedAt = now;
  }
  // Debounce press/release transitions without delaying normal taps. The UI
  // itself ignores ambiguous gestures, so this does not reintroduce view flips.
  constexpr uint32_t DEBOUNCE_MS = 35;
  if (rawDown && now - rawChangedAt >= DEBOUNCE_MS) {
    stableDown = true;
    lastPoint = rawPoint;
  }
  if (stableDown && rawDown) {
    lastPoint = rawPoint;
    d->point = lastPoint;
    d->state = LV_INDEV_STATE_PRESSED;
  } else if (!rawDown && now - rawChangedAt >= DEBOUNCE_MS) {
    stableDown = false;
  } else if (stableDown) {
    // Keep the last known coordinate during the release debounce interval.
    d->point = lastPoint;
    d->state = LV_INDEV_STATE_PRESSED;
  }
}
} // namespace touch
