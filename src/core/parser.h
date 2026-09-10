#pragma once
#include "model.h"
#include <functional>
// get() returns next byte or -1 on EOF/timeout. At most 2 MiB per response.
bool parseAircraft(const std::function<int()> &get, const Settings &settings, Snapshot &out,
                   char *error, size_t errorSize);
