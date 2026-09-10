#include "ui/ui.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t pixels[466 * 466];
static bool pressed = false;
static int mx = 0, my = 0;
int main(int argc, char **argv) {
  bool capture = argc > 1;
  bool captureDetails = argc > 3 && !strcmp(argv[3], "details");
  SDL_Init(SDL_INIT_VIDEO);
  auto window = SDL_CreateWindow("FlightDot | SIMULACAO", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, 466, 466, 0);
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, 466, 466);
  lv_init();
  static lv_color_t buffer[466 * 40];
  static lv_disp_draw_buf_t buf;
  lv_disp_draw_buf_init(&buf, buffer, nullptr, 466 * 40);
  static lv_disp_drv_t drv;
  lv_disp_drv_init(&drv);
  drv.hor_res = 466;
  drv.ver_res = 466;
  drv.draw_buf = &buf;
  drv.flush_cb = [](lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p) {
    for (int y = a->y1; y <= a->y2; y++)
      for (int x = a->x1; x <= a->x2; x++) {
        uint16_t v = p->full;
        pixels[y * 466 + x] = (v >> 8) | (v << 8);
        p++;
      }
    lv_disp_flush_ready(d);
  };
  lv_disp_drv_register(&drv);
  static lv_indev_drv_t in;
  lv_indev_drv_init(&in);
  in.type = LV_INDEV_TYPE_POINTER;
  in.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) {
    d->point.x = mx;
    d->point.y = my;
    d->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  };
  lv_indev_drv_register(&in);
  Settings s;
  if (argc > 2)
    s.theme = std::clamp(atoi(argv[2]), 0, 4);
  ui::begin(s);
  Status status;
  status.wifi = true;
  snprintf(status.clock, sizeof(status.clock), "20:45:32");
  snprintf(status.date, sizeof(status.date), "09/09");
  ui::setStatus(status);
  Snapshot data;
  mockSnapshot(data, s, 0);
  ui::update(data);
  bool run = true;
  int injected = 0;
  uint32_t last = SDL_GetTicks(), refresh = 0, start = last;
  while (run) {
    uint32_t now = SDL_GetTicks(), elapsed = now - start;
    if (captureDetails && !injected && elapsed > 3200) {
      float angle = data.planes[0].bearing * .01745329252f;
      float radius = data.planes[0].distance / s.rangeKm * 228;
      mx = 233 + sin(angle) * radius;
      my = 233 - cos(angle) * radius;
      pressed = true;
      injected = 1;
    } else if (captureDetails && injected == 1 && elapsed > 3340) {
      pressed = false;
      injected = 2;
    }
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        run = false;
      if (e.type == SDL_MOUSEBUTTONDOWN)
        pressed = true;
      if (e.type == SDL_MOUSEBUTTONUP)
        pressed = false;
      if (e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP)
        SDL_GetMouseState(&mx, &my);
    }
    Settings req;
    Aircraft a;
    auto action = ui::takeAction(req, a);
    if (action == ui::Action::SettingsChanged) {
      s = req;
      mockSnapshot(data, s, elapsed);
      ui::update(data);
    }
    if (action == ui::Action::RouteRequested) {
      Route route;
      snprintf(route.hex, sizeof(route.hex), "%s", a.hex);
      snprintf(route.airline, sizeof(route.airline), "Companhia Demonstracao");
      snprintf(route.origin, sizeof(route.origin), "SBSR");
      snprintf(route.destination, sizeof(route.destination), "SBGR");
      snprintf(route.destinationCity, sizeof(route.destinationCity), "Guarulhos");
      snprintf(route.type, sizeof(route.type), "Airbus A320");
      route.originLat = -20.4697;
      route.originLon = -54.6703;
      route.destinationLat = -23.4356;
      route.destinationLon = -46.4731;
      route.temperature = 24.3;
      route.found = true;
      ui::setRoute(route);
    }
    if (elapsed - refresh >= 5000) {
      refresh = elapsed;
      mockSnapshot(data, s, elapsed);
      ui::update(data);
    }
    lv_tick_inc(now - last);
    last = now;
    ui::tick(elapsed);
    lv_timer_handler();
    SDL_UpdateTexture(texture, nullptr, pixels, 466 * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    uint32_t captureAt = captureDetails ? 4400 : (argc > 3 && !strcmp(argv[3], "splash") ? 1600 : 3500);
    if (capture && elapsed > captureAt) {
      SDL_Surface *surface =
          SDL_CreateRGBSurfaceWithFormatFrom(pixels, 466, 466, 16, 466 * 2, SDL_PIXELFORMAT_RGB565);
      SDL_SaveBMP(surface, argv[1]);
      SDL_FreeSurface(surface);
      run = false;
    }
    SDL_Delay(5);
  }
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
