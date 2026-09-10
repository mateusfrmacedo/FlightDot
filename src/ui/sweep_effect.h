#pragma once
#include <lvgl.h>
namespace ui {
void drawScope(lv_draw_ctx_t *context, uint32_t grid, uint32_t beam, uint32_t now, bool sweep,
               bool dots = false);
} // namespace ui
