#pragma once
#include "model.h"
namespace storage {
Settings load();
bool save(const Settings &);
bool readRoute(const char *hex, const char *callsign, Route &);
void writeRoute(const Route &);
} // namespace storage
