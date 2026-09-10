#include "sweep_effect.h"
#include <algorithm>
#include <cmath>

namespace ui {
namespace {
constexpr int SIZE = 466, CENTER = 233, RADIUS = 228;
constexpr int ANGLES = 1024, LEVELS = 256, GRID_LEVELS = 16;
constexpr uint32_t REVOLUTION_MS = 6000;
// Packed geometry: 10 angle bits, 4 grid-coverage bits, 1 sweep-coverage bit.
// Geometry is allocated in PSRAM.
uint16_t *geometry = nullptr;
lv_color_t palette[GRID_LEVELS][LEVELS];
uint16_t fade[ANGLES];
uint32_t lastGrid = 0, lastBeam = 0;

void prepareGeometry() {
  if (geometry)
    return;
  geometry = static_cast<uint16_t *>(lv_mem_alloc(SIZE * SIZE * sizeof(uint16_t)));
  if (!geometry)
    return;
  for (int y = 0; y < SIZE; ++y) {
    for (int x = 0; x < SIZE; ++x) {
      float dx = x - CENTER, dy = y - CENTER;
      float radius = sqrtf(dx * dx + dy * dy), coverage = 0;
      if (radius <= RADIUS + 3) {
        for (int ring = 57; ring <= RADIUS; ring += 57) {
          float halfWidth = ring == RADIUS ? 3.f : 2.5f;
          coverage =
              std::max(coverage, std::clamp(halfWidth + .5f - fabsf(radius - ring), 0.f, 1.f));
        }
        if (radius <= RADIUS) {
          coverage = std::max(coverage, std::clamp(3.f - std::min(fabsf(dx), fabsf(dy)), 0.f, 1.f));
          float diagonal = std::min(fabsf(dx - dy), fabsf(dx + dy)) * .70710678f;
          coverage = std::max(coverage, std::clamp(3.f - diagonal, 0.f, 1.f) * .72f);
        }
      }
      int angle = int(lroundf(atan2f(dx, -dy) * (ANGLES / 6.283185307f))) & (ANGLES - 1);
      unsigned band = radius <= RADIUS - 2 ? 1 : 0;
      geometry[y * SIZE + x] = angle | (int(lroundf(coverage * 15)) << 10) | (band << 14);
    }
  }
  // Video reference: bright narrow front, continuous ~55-degree afterglow.
  constexpr float trail = ANGLES * 55.f / 360.f;
  for (int i = 0; i < ANGLES; ++i) {
    float alpha = 0;
    if (i < trail) {
      float t = 1.f - i / trail;
      alpha = .58f * t * t;
    }
    int edge = std::min(i, ANGLES - i);
    alpha = std::max(alpha, .32f * expf(-edge * edge / 18.f));
    if (edge <= 1)
      alpha = 1.f;
    fade[i] = uint16_t(lroundf(std::clamp(alpha, 0.f, 1.f) * (LEVELS - 1) * 16));
  }
}
void preparePalette(uint32_t grid, uint32_t beam) {
  if (lastGrid == grid && lastBeam == beam)
    return;
  lastGrid = grid;
  lastBeam = beam;
  lv_color_t ink = lv_color_hex(beam);
  for (int g = 0; g < GRID_LEVELS; ++g) {
    lv_color_t base = lv_color_mix(lv_color_hex(grid), lv_color_black(), g * 255 / 15);
    for (int level = 0; level < LEVELS; ++level) {
      palette[g][level] = lv_color_mix(ink, base, level);
    }
  }
}
} // namespace
void drawScope(lv_draw_ctx_t *context, uint32_t grid, uint32_t beam, uint32_t now, bool sweep,
               bool dots) {
  prepareGeometry();
  if (!geometry)
    return;
  preparePalette(grid, beam);
  const lv_area_t &clip = *context->clip_area, &buffer = *context->buf_area;
  int x1 = std::max(0, int(clip.x1)), x2 = std::min(SIZE - 1, int(clip.x2));
  int y1 = std::max(0, int(clip.y1)), y2 = std::min(SIZE - 1, int(clip.y2));
  int stride = lv_area_get_width(&buffer);
  int phase = (now % REVOLUTION_MS) * ANGLES / REVOLUTION_MS;
  // Paint only the current LVGL clip into its RGB565 draw buffer. The hot path
  // uses lookups, without transforms, masks, trig or per-pixel alpha arithmetic.
  for (int y = y1; y <= y2; ++y) {
    auto *out = static_cast<lv_color_t *>(context->buf) + (y - buffer.y1) * stride + x1 - buffer.x1;
    const uint16_t *sample = geometry + y * SIZE + x1;
    for (int x = x1; x <= x2; ++x) {
      unsigned value = *sample++;
      unsigned band = value >> 14, g = (value >> 10) & 15;
      if (dots) {
        // Keep the outer bezel and replace rings/crosshairs with a regular
        // phosphor dot matrix. CENTER is itself a dot.
        // C++ keeps a negative remainder for coordinates left/above CENTER.
        // Normalize it so the dot matrix covers all four quadrants uniformly.
        int dx = abs(((x - CENTER + 12) % 24 + 24) % 24 - 12);
        int dy = abs(((y - CENTER + 12) % 24 + 24) % 24 - 12);
        g = !band ? g : (dx * dx + dy * dy <= 5 ? 15 : 0);
      }
      unsigned opacity = sweep && band ? fade[(phase - value) & (ANGLES - 1)] : 0;
      // Ordered subpixel dithering reduces visible RGB565 steps in the afterglow.
      constexpr uint8_t dither[4] = {0, 32, 48, 16};
      unsigned level =
          opacity ? std::min(255u, (opacity + dither[(x & 1) | ((y & 1) << 1)]) >> 4) : 0;
      *out++ = palette[g][level];
    }
  }
}
} // namespace ui
