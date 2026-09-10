#pragma once
#include <stdint.h>
namespace display {
void begin();
void brightness(uint8_t value);
const char *controller();
void testScreen();
uint32_t frameCount();
} // namespace display
