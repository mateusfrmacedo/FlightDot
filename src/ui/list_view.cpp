#include "ui.h"
#include <cstdio>
namespace ui {
void list(lv_draw_ctx_t *c, const Snapshot &d, int page, uint32_t color) {
  text(c, 80, 88, 306, "AERONAVES PRÓXIMAS", color, 18, LV_TEXT_ALIGN_CENTER);
  for (size_t j = 0; j < 7; j++) {
    size_t i = page * 7 + j;
    if (i >= d.count)
      break;
    auto &a = d.planes[i];
    char row[96];
    snprintf(row, sizeof(row), "%s  %.0f km  %.0f ft", a.callsign[0] ? a.callsign : a.hex,
             a.distance, a.altitude);
    text(c, 70, 125 + j * 33, 330, row, emergency(a) ? 0xff6262 : color, 14);
  }
  char p[48];
  snprintf(p, sizeof(p), "Página %d/%d | toque aqui", page + 1,
           int((d.count + 6) / 7 ? (d.count + 6) / 7 : 1));
  text(c, 85, 365, 296, p, 0x82938a, 12, LV_TEXT_ALIGN_CENTER);
}
} // namespace ui
