#pragma once
#include "../core/model.h"
namespace adsb {
void initMemory();
void begin(const Settings &);
void configure(const Settings &);
bool follow(const char *query);
void following(char *, size_t);
bool snapshot(Snapshot &);
void requestRoute(const Aircraft &);
bool route(Route &);
void error(char *, size_t);
} // namespace adsb
