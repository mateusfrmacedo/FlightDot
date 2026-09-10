#pragma once
// Waveshare ESP32-S3-Touch-AMOLED-1.43, official Demo V3 / schematic.
namespace pins {
constexpr int cs = 9, clock = 10, d0 = 11, d1 = 12, d2 = 13, d3 = 14;
constexpr int reset = 21, oledEnable = 42, sda = 47, scl = 48;
constexpr int voltage = 4, imuInterrupt = 8, rtcInterrupt = 15;
constexpr int width = 466, height = 466;
} // namespace pins
