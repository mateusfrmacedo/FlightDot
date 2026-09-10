#pragma once
#include "../core/model.h"
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(font_pt_16);
LV_FONT_DECLARE(font_pt_18);
LV_FONT_DECLARE(font_pt_24);
LV_FONT_DECLARE(font_pt_28);
#ifdef __cplusplus
}
#endif
namespace ui {
enum class Action { None, SettingsChanged, RouteRequested };
void begin(const Settings &s);
void update(const Snapshot &s);
void setStatus(const Status &s);
void setSettings(const Settings &s);
void setRoute(const Route &r);
void tick(uint32_t now);
Action takeAction(Settings &settings, Aircraft &aircraft);
uint32_t lastTouch();
// Shared drawing primitives, also used by desktop simulator.
void text(lv_draw_ctx_t *ctx, int x, int y, int width, const char *value, uint32_t color,
          int size = 14, lv_text_align_t align = LV_TEXT_ALIGN_LEFT);
void line(lv_draw_ctx_t *ctx, int x, int y, int xx, int yy, uint32_t color, int width = 1,
          int opacity = 255);
void circle(lv_draw_ctx_t *ctx, int x, int y, int radius, uint32_t color, int width = 1,
            int opacity = 255);
void list(lv_draw_ctx_t *ctx, const Snapshot &data, int page, uint32_t color);
void statistics(lv_draw_ctx_t *ctx, const Snapshot &data, int rangeKm, uint32_t color);
void details(lv_draw_ctx_t *ctx, const Aircraft &a, const Route &route, uint32_t color);
} // namespace ui
