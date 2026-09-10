#pragma once
#include <lvgl.h>
namespace touch {
void begin();
void read(lv_indev_drv_t *, lv_indev_data_t *);
} // namespace touch
