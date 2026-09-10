#include "../core/places.h"
#include "sweep_effect.h"
#include "ui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace ui {
static constexpr int SCOPE_RADIUS = 228;
static Settings cfg;
static Snapshot data, previous;
static Status status;
static Route route;
static Aircraft selected;
static lv_obj_t *root;
static uint32_t timeMs = 0, received = 0, touched = 0, pressed = 0, lastTap = 0;
static lv_point_t down;
static int view = 0, page = 0;
static bool detail = false;
static Action pending = Action::None;
static float pointX[MAX_AIRCRAFT], pointY[MAX_AIRCRAFT];
static float startX[MAX_AIRCRAFT], startY[MAX_AIRCRAFT];
static bool hasStart[MAX_AIRCRAFT];
static VisiblePlace landmarks[56];
static size_t landmarkCount = 0;
static double landmarkLat = NAN, landmarkLon = NAN;
static int landmarkRange = -1;
static uint32_t splashStart = 0;
static bool splashDone = false;

static uint32_t themeGrid() {
  const uint32_t colors[] = {0x185e42, 0x684a12, 0x691d26, 0x173d75, 0x087a42};
  return colors[std::clamp(cfg.theme, 0, 4)];
}
static uint32_t themeBeam() {
  const uint32_t colors[] = {0x76ff73, 0xffc257, 0xff354d, 0x48a6ff, 0x00ff72};
  return colors[std::clamp(cfg.theme, 0, 4)];
}
static uint32_t themeText() {
  const uint32_t colors[] = {0x73ffab, 0xffb64a, 0xff6273, 0x78beff, 0x36ff91};
  return colors[std::clamp(cfg.theme, 0, 4)];
}
void text(lv_draw_ctx_t *c, int x, int y, int w, const char *value, uint32_t color, int size,
          lv_text_align_t align) {
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.color = lv_color_hex(color);
  d.font = size == 24   ? &font_pt_28
           : size == 18 ? &font_pt_24
           : size == 12 ? &font_pt_16
                        : &font_pt_18;
  d.align = align;
  lv_area_t a = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)(x + w - 1), 465};
  lv_draw_label(c, &d, &a, value, nullptr);
}
void line(lv_draw_ctx_t *c, int x, int y, int xx, int yy, uint32_t color, int width, int opacity) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.color = lv_color_hex(color);
  d.width = width;
  d.opa = opacity;
  lv_point_t a = {(lv_coord_t)x, (lv_coord_t)y}, b = {(lv_coord_t)xx, (lv_coord_t)yy};
  lv_draw_line(c, &d, &a, &b);
}
void circle(lv_draw_ctx_t *c, int x, int y, int r, uint32_t color, int width, int opacity) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.color = lv_color_hex(color);
  d.width = width;
  d.opa = opacity;
  lv_point_t p = {(lv_coord_t)x, (lv_coord_t)y};
  lv_draw_arc(c, &d, &p, r, 0, 360);
}
static void position(const Aircraft &a, float &x, float &y) {
  float angle = a.bearing * .01745329252f, rad = a.distance / cfg.rangeKm * SCOPE_RADIUS;
  x = 233 + sin(angle) * rad;
  y = 233 - cos(angle) * rad;
}
static void geographicPosition(double lat, double lon, float &x, float &y) {
  double centerLat = data.following ? data.centerLat : cfg.lat;
  double centerLon = data.following ? data.centerLon : cfg.lon;
  float distance = distanceKm(centerLat, centerLon, lat, lon);
  float angle = bearingDeg(centerLat, centerLon, lat, lon) * .01745329252f;
  float radius = distance / cfg.rangeKm * SCOPE_RADIUS;
  x = 233 + sin(angle) * radius;
  y = 233 - cos(angle) * radius;
}
static void refreshLandmarks(bool force = false) {
  double lat = data.following ? data.centerLat : cfg.lat;
  double lon = data.following ? data.centerLon : cfg.lon;
  if (!force && landmarkRange == cfg.rangeKm && std::isfinite(landmarkLat) &&
      distanceKm(lat, lon, landmarkLat, landmarkLon) < 2.f)
    return;
  landmarkCount = nearbyPlaces(lat, lon, cfg.rangeKm, landmarks, 56);
  landmarkLat = lat;
  landmarkLon = lon;
  landmarkRange = cfg.rangeKm;
}
static size_t renderedAircraftCount() {
  // At wide ranges the API can return far more labels than a 466 px display can
  // present. Keep the nearest aircraft (the client sorts them by distance) and
  // preserve the complete count for the Statistics view.
  size_t limit = cfg.rangeKm >= 200 ? 40 : cfg.rangeKm >= 150 ? 60 : MAX_AIRCRAFT;
  return std::min(data.count, limit);
}
static void radar(lv_draw_ctx_t *c, uint32_t color) {
  uint32_t grid = themeGrid();
  drawScope(c, grid, themeBeam(), timeMs, cfg.sweep, false);
  text(c, 218, 14, 30, "N", color, 18, LV_TEXT_ALIGN_CENTER);
  text(c, 218, 431, 30, "S", color, 18, LV_TEXT_ALIGN_CENTER);
  text(c, 432, 221, 28, "L", color, 18, LV_TEXT_ALIGN_CENTER);
  text(c, 6, 221, 28, "O", color, 18, LV_TEXT_ALIGN_CENTER);
  lv_area_t placeLabels[16];
  size_t placeLabelCount = 0;
  for (size_t i = 0; i < landmarkCount; ++i) {
      const auto &p = landmarks[i];
      if (p.kind != 'A' || placeLabelCount >= 8)
        continue;
      float x, y;
      geographicPosition(p.lat, p.lon, x, y);
      if (y < 82 || y > 365)
        continue;
      char airportLabel[64];
      snprintf(airportLabel, sizeof(airportLabel), "%s  %s", p.name, p.code);
      const char *label = airportLabel;
      int width = 190;
      int candidates[][2] = {{int(x) + 8, int(y) - 9},
                             {int(x) - width - 8, int(y) - 9},
                             {int(x) - width / 2, int(y) + 8},
                             {int(x) - width / 2, int(y) - 27}};
      for (auto &candidate : candidates) {
        int lx = std::clamp(candidate[0], 20, 446 - width), ly = candidate[1];
        lv_area_t box = {(lv_coord_t)lx, (lv_coord_t)ly, (lv_coord_t)(lx + width),
                         (lv_coord_t)(ly + 21)};
        bool fits = ly >= 82 && ly <= 365;
        for (size_t j = 0; j < placeLabelCount; ++j)
          if (!(box.x2 < placeLabels[j].x1 || box.x1 > placeLabels[j].x2 ||
                box.y2 < placeLabels[j].y1 || box.y1 > placeLabels[j].y2))
            fits = false;
        if (fits) {
          placeLabels[placeLabelCount++] = box;
          text(c, lx, ly, width, label, 0xffe39a, 12);
          break;
        }
      }
  }
  circle(c, 233, 233, 4, color, 3);
  if (!data.following && cfg.centerName[0])
    text(c, 242, 240, 150, cfg.centerName, 0xe2ffec, 12);

  float t = std::clamp((timeMs - received) / 4800.f, 0.f, 1.f);
  lv_area_t labels[32];
  memcpy(labels, placeLabels, placeLabelCount * sizeof(lv_area_t));
  size_t labelCount = placeLabelCount;
  size_t renderCount = renderedAircraftCount();
  size_t aircraftLabelLimit = cfg.rangeKm >= 200 ? 16 : 24;
  for (size_t i = 0; i < renderCount; i++) {
    const auto &a = data.planes[i];
    float x, y;
    position(a, x, y);
    float oldx = hasStart[i] ? startX[i] : x;
    float oldy = hasStart[i] ? startY[i] : y;
    x = oldx + (x - oldx) * t;
    y = oldy + (y - oldy) * t;
    pointX[i] = x;
    pointY[i] = y;
    uint32_t tint = emergency(a)         ? 0xff5353
                    : cfg.theme == 1     ? 0xffbb4b
                    : cfg.theme == 2     ? 0xff7180
                    : cfg.theme == 3     ? 0x83c8ff
                    : cfg.theme == 4     ? 0x35ff88
                    : a.altitude < 10000 ? 0xffdf72
                    : a.altitude < 25000 ? 0x68ffa1
                                         : 0xaddfff;
    float h = std::isfinite(a.heading) ? a.heading * .01745329252f : 0;
    float sx = sin(h), cy = cos(h);
    auto seg = [&](float ax, float ay, float bx, float by) {
      ax *= 1.5f;
      ay *= 1.5f;
      bx *= 1.5f;
      by *= 1.5f;
      line(c, x + ax * cy - ay * sx, y + ax * sx + ay * cy, x + bx * cy - by * sx,
           y + bx * sx + by * cy, tint, 3);
    };
    if (!strcmp(a.category, "A7")) {
      seg(-11, -5, 11, -5);
      seg(-6, 1, 6, 1);
      seg(0, -5, 0, 9);
      seg(-6, 9, 6, 9);
    } else if (!strcmp(a.category, "B2")) {
      circle(c, x, y - 4, 11, tint, 3);
      seg(-4, 7, 4, 7);
      seg(-4, 7, -2, 12);
      seg(4, 7, 2, 12);
    } else if (!strcmp(a.category, "B6")) {
      seg(-9, -9, 9, 9);
      seg(9, -9, -9, 9);
      circle(c, x - 14, y - 14, 4, tint, 2);
      circle(c, x + 14, y - 14, 4, tint, 2);
      circle(c, x - 14, y + 14, 4, tint, 2);
      circle(c, x + 14, y + 14, 4, tint, 2);
    } else {
      seg(0, -10, 0, 9);
      seg(-8, 2, 0, -3);
      seg(0, -3, 8, 2);
      seg(-4, 9, 0, 6);
      seg(0, 6, 4, 9);
    }
    if (emergency(a))
      circle(c, x, y, 21, tint, 2, (timeMs / 400) % 2 ? 255 : 90);
    if (i < aircraftLabelLimit && labelCount < 32) {
      int candidates[][2] = {{int(x) + 19, int(y) - 9},
                             {int(x) - 123, int(y) - 9},
                             {int(x) - 52, int(y) + 22},
                             {int(x) - 52, int(y) - 38}};
      for (auto &candidate : candidates) {
        int lx = candidate[0], ly = candidate[1];
        lv_area_t box = {(lv_coord_t)lx, (lv_coord_t)ly, (lv_coord_t)(lx + 104),
                         (lv_coord_t)(ly + 20)};
        bool fits = ly >= 80 && ly < 375;
        for (auto corner : {lv_point_t{box.x1, box.y1}, lv_point_t{box.x2, box.y2}})
          if ((corner.x - 233) * (corner.x - 233) + (corner.y - 233) * (corner.y - 233) > 215 * 215)
            fits = false;
        for (size_t j = 0; j < labelCount; j++)
          if (!(box.x2 < labels[j].x1 || box.x1 > labels[j].x2 || box.y2 < labels[j].y1 ||
                box.y1 > labels[j].y2))
            fits = false;
        if (fits) {
          labels[labelCount++] = box;
          text(c, lx, ly, 105,
               a.callsign[0]       ? a.callsign
               : a.registration[0] ? a.registration
                                   : a.hex,
               tint, 12);
          break;
        }
      }
    }
  }
}
static uint32_t fadeColor(uint32_t color, unsigned alpha) {
  unsigned r = ((color >> 16) & 255) * alpha / 255;
  unsigned g = ((color >> 8) & 255) * alpha / 255;
  unsigned b = (color & 255) * alpha / 255;
  return (r << 16) | (g << 8) | b;
}
static void splash(lv_draw_ctx_t *c) {
  uint32_t elapsed = timeMs - splashStart;
  unsigned alpha = elapsed < 450 ? elapsed * 255 / 450
                   : elapsed > 2200
                       ? std::max(0, 2800 - int(elapsed)) * 255 / 600
                       : 255;
  uint32_t color = fadeColor(0x66ff9b, alpha);
  text(c, 58, 174, 350, "FlightDot", color, 24, LV_TEXT_ALIGN_CENTER);
  text(c, 100, 214, 266, "LIVE ADS-B", fadeColor(0xb7c9be, alpha), 12,
       LV_TEXT_ALIGN_CENTER);
  float progress = std::clamp((int(elapsed) - 300) / 1800.f, 0.f, 1.f);
  int x = 58 + int(progress * 350), y = 270;
  line(c, x, y - 15, x, y + 15, color, 4);
  line(c, x - 17, y + 2, x, y - 5, color, 4);
  line(c, x, y - 5, x + 17, y + 2, color, 4);
  line(c, x - 8, y + 14, x, y + 9, color, 3);
  line(c, x, y + 9, x + 8, y + 14, color, 3);
  for (int i = 1; i <= 4; ++i)
    line(c, x - i * 20, y + 15, x - i * 20 - 12, y + 15, color, 2, 220 / i);
}
static void draw(lv_event_t *e) {
  auto c = lv_event_get_draw_ctx(e);
  uint32_t color = themeText();
  if (!splashDone) {
    splash(c);
    return;
  }
  if (detail)
    details(c, selected, route, color);
  else if (view == 0)
    radar(c, color);
  else if (view == 1)
    list(c, data, page, color);
  else
    statistics(c, data, cfg.rangeKm, color);
  if (detail)
    return;
  // HUD text overlays the scope directly, without background boxes.
  char b[100];
  text(c, 100, 42, 266, status.clock, color, 14, LV_TEXT_ALIGN_CENTER);
  text(c, 100, 65, 266, status.date, 0x819589, 12, LV_TEXT_ALIGN_CENTER);
  const char *foot = data.mock      ? "DEMONSTRACAO - DADOS SIMULADOS"
                     : status.setup ? "FlightDot-Setup / 192.168.4.1"
                     : !status.wifi ? "SEM CONEXAO"
                     : !data.valid  ? "AGUARDANDO DADOS"
                     : timeMs - data.updatedMs > uint32_t(cfg.staleSeconds * 1000)
                         ? "DADOS ATRASADOS"
                         : "";
  if (status.follow[0] && status.wifi && !data.mock) {
    snprintf(b, sizeof(b), "%s %.8s",
             !data.valid                                                   ? "BUSCANDO"
             : timeMs - data.updatedMs > uint32_t(cfg.staleSeconds * 1000) ? "SEM SINAL"
                                                                           : "SEGUINDO",
             status.follow);
    foot = b;
  }
  text(c, 60, 407, 346, foot,
       (data.mock || !data.valid || !status.wifi ||
        timeMs - data.updatedMs > uint32_t(cfg.staleSeconds * 1000))
           ? 0xffab56
           : 0x819589,
       12, LV_TEXT_ALIGN_CENTER);
}
static void input(lv_event_t *e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_get_act(), &p);
  touched = timeMs;
  if (!splashDone)
    return;
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    down = p;
    pressed = timeMs;
    return;
  }
  if (lv_event_get_code(e) != LV_EVENT_RELEASED)
    return;
  lv_obj_invalidate(root);
  int dx = p.x - down.x, dy = p.y - down.y;
  uint32_t held = timeMs - pressed;
  if (abs(dx) > 65 || abs(dy) > 65) {
    detail = false;
    view = (view + (dx < 0 ? 1 : 2)) % 3;
    lastTap = 0;
    return;
  }
  if (held > 750) {
    cfg.theme = (cfg.theme + 1) % 5;
    pending = Action::SettingsChanged;
    lastTap = 0;
    return;
  }
  if (detail) {
    detail = false;
    lastTap = 0;
    return;
  }
  if (lastTap && timeMs - lastTap < 330) {
    int presets[] = {50, 100, 150, 250};
    int next = 0;
    for (int i = 0; i < 4; i++)
      if (cfg.rangeKm == presets[i])
        next = (i + 1) % 4;
    cfg.rangeKm = presets[next];
    pending = Action::SettingsChanged;
    lastTap = 0;
    return;
  }
  lastTap = timeMs;
  if (view == 1 && p.y > 355 && p.y < 396) {
    page = (page + 1) % std::max(1, int((data.count + 6) / 7));
    return;
  }
  int hit = -1;
  float best = 30 * 30;
  if (view == 0)
    for (size_t i = 0; i < renderedAircraftCount(); i++) {
      float d = (p.x - pointX[i]) * (p.x - pointX[i]) + (p.y - pointY[i]) * (p.y - pointY[i]);
      if (d < best) {
        best = d;
        hit = i;
      }
    }
  if (view == 1 && p.y >= 125 && p.y < 356) {
    int idx = page * 7 + (p.y - 125) / 33;
    if (idx < int(data.count))
      hit = idx;
  }
  if (hit >= 0) {
    selected = data.planes[hit];
    route = {};
    detail = true;
    pending = Action::RouteRequested;
  }
}
void begin(const Settings &s) {
  cfg = s;
  refreshLandmarks(true);
  root = lv_scr_act();
  lv_obj_set_style_bg_color(root, lv_color_black(), 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(root, draw, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(root, input, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(root, input, LV_EVENT_RELEASED, nullptr);
}
void update(const Snapshot &s) {
  previous = data;
  data = s;
  memset(hasStart, 0, sizeof(hasStart));
  if (!s.following) {
    for (size_t i = 0; i < renderedAircraftCount(); ++i) {
      for (size_t j = 0; j < previous.count; ++j) {
        if (strcmp(data.planes[i].hex, previous.planes[j].hex))
          continue;
        position(previous.planes[j], startX[i], startY[i]);
        hasStart[i] = true;
        break;
      }
    }
  }
  refreshLandmarks();
  if (s.following)
    clearSnapshot(previous);
  if (detail)
    for (size_t i = 0; i < s.count; i++)
      if (!strcmp(selected.hex, s.planes[i].hex))
        selected = s.planes[i];
  received = timeMs;
  page = std::min(page, std::max(0, int((s.count + 6) / 7) - 1));
  lv_obj_invalidate(root);
}
void setStatus(const Status &s) {
  if (strcmp(status.follow, s.follow)) {
    clearSnapshot(data);
    clearSnapshot(previous);
    detail = false;
    refreshLandmarks();
    lv_obj_invalidate(root);
  }
  status = s;
  // These are damage rectangles only; they do not paint a background box.
  lv_area_t top = {90, 35, 376, 87}, bottom = {40, 380, 430, 428};
  lv_obj_invalidate_area(root, &top);
  lv_obj_invalidate_area(root, &bottom);
}
void setSettings(const Settings &s) {
  if (s.lat != cfg.lat || s.lon != cfg.lon || s.rangeKm != cfg.rangeKm) {
    clearSnapshot(previous);
    clearSnapshot(data);
    detail = false;
  }
  cfg = s;
  refreshLandmarks(true);
  lv_obj_invalidate(root);
}
void setRoute(const Route &r) {
  if (!strcmp(r.hex, selected.hex)) {
    route = r;
    lv_obj_invalidate(root);
  }
}
static void invalidateSweep(uint32_t at) {
  float front = (at % 6000) * (6.283185307f / 6000);
  float minX = 233, maxX = 233, minY = 233, maxY = 233;
  // Include the afterglow, the bright front and its faint forward halo.
  for (int i = 0; i <= 64; i++) {
    float a = front + (-56.f + i * 60.f / 64) * .01745329252f;
    float x = 233 + sin(a) * 226, y = 233 - cos(a) * 226;
    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
  }
  lv_area_t area = {(lv_coord_t)std::max(0, int(floorf(minX)) - 4),
                    (lv_coord_t)std::max(0, int(floorf(minY)) - 4),
                    (lv_coord_t)std::min(465, int(ceilf(maxX)) + 4),
                    (lv_coord_t)std::min(465, int(ceilf(maxY)) + 4)};
  lv_obj_invalidate_area(root, &area);
}
void tick(uint32_t now) {
  timeMs = now;
  static uint32_t last = 0;
  if (!splashStart)
    splashStart = now;
  if (!splashDone) {
    if (now - splashStart < 2800) {
      if (now - last >= 33) {
        lv_area_t area = {35, 150, 431, 310};
        lv_obj_invalidate_area(root, &area);
        last = now;
      }
      return;
    }
    splashDone = true;
    lv_obj_invalidate(root);
  }
  if (now - last >= 33) {
    if (!detail && view == 0) {
      if (cfg.sweep) {
        invalidateSweep(last);
        invalidateSweep(now);
      }
      {
        for (size_t i = 0; i < renderedAircraftCount(); i++) {
          if (!(now - received < 5000 && previous.valid) &&
              !(emergency(data.planes[i]) && now / 400 != last / 400))
            continue;
          float x, y;
          position(data.planes[i], x, y);
          float oldx = hasStart[i] ? startX[i] : x;
          float oldy = hasStart[i] ? startY[i] : y;
          lv_area_t area = {(lv_coord_t)std::max(0, int(std::min(oldx, x)) - 130),
                            (lv_coord_t)std::max(0, int(std::min(oldy, y)) - 46),
                            (lv_coord_t)std::min(465, int(std::max(oldx, x)) + 130),
                            (lv_coord_t)std::min(465, int(std::max(oldy, y)) + 48)};
          lv_obj_invalidate_area(root, &area);
        }
      }
    }
    last = now;
  }
}
Action takeAction(Settings &s, Aircraft &a) {
  Action x = pending;
  pending = Action::None;
  s = cfg;
  a = selected;
  return x;
}
uint32_t lastTouch() {
  return touched;
}
} // namespace ui
