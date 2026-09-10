#pragma once
#include "../core/model.h"
namespace webConfig {
void begin(Settings &);
void tick();
bool changed();
void status(const Snapshot &, const Status &);
} // namespace webConfig
