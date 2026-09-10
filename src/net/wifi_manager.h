#pragma once
namespace wifiManager {
void begin();
void tick();
bool portal();
void connect(const char *, const char *);
void reset();
} // namespace wifiManager
