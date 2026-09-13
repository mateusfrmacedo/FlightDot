#include "touch.h"
#include "board_pins.h"
#include <Arduino.h>
#include <Wire.h>
namespace touch {
namespace {
constexpr uint8_t FT3168_ADDRESS = 0x38;
constexpr uint8_t TOUCH_COUNT = 0x02;
constexpr uint8_t TOUCH_XY = 0x03;
static lv_point_t lastPoint{233, 233};

// Waveshare Demo V3 reads the touch count first, then exactly four bytes of
// coordinates. It does not add software debouncing to the FT3168 data stream.
static bool readRegister(uint8_t reg, uint8_t *buffer, size_t length) {
  Wire.beginTransmission(FT3168_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return false;
  if (Wire.requestFrom(FT3168_ADDRESS, static_cast<uint8_t>(length)) != length)
    return false;
  for (size_t i = 0; i < length; ++i)
    buffer[i] = Wire.read();
  return true;
}
} // namespace

void begin() {
  // Official configuration for ESP32-S3-Touch-AMOLED-1.43: GPIO 47/48, 300 kHz.
  Wire.begin(pins::sda, pins::scl, 300000);
  Wire.setTimeOut(20);
  uint8_t id = 0;
  bool ready = readRegister(0xA3, &id, 1);
  if (ready) {
    Wire.beginTransmission(FT3168_ADDRESS);
    Wire.write(uint8_t(0x00));
    Wire.write(uint8_t(0x00)); // normal/active mode, as in the official demo
    ready = Wire.endTransmission() == 0;
  }
  Serial.printf("[touch] FT3168 @0x38, GPIO 47/48, 300 kHz: %s\n", ready ? "ready" : "not found");
}

void read(lv_indev_drv_t *, lv_indev_data_t *data) {
  data->point = lastPoint;
  data->state = LV_INDEV_STATE_RELEASED;

  uint8_t count = 0;
  if (!readRegister(TOUCH_COUNT, &count, 1) || (count & 0x0f) == 0)
    return;

  uint8_t raw[4];
  if (!readRegister(TOUCH_XY, raw, sizeof(raw)))
    return;
  int x = ((raw[0] & 0x0f) << 8) | raw[1];
  int y = ((raw[2] & 0x0f) << 8) | raw[3];
  if (x >= pins::width || y >= pins::height)
    return;

  // Match the rotation performed by the DMA display flush (USB-C at the bottom).
  lastPoint.x = y;
  lastPoint.y = pins::height - 1 - x;
  data->point = lastPoint;
  data->state = LV_INDEV_STATE_PRESSED;
}
} // namespace touch
