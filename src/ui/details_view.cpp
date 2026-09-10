#include "ui.h"
#include <cstdio>
#include <cstring>
#include <ctime>
namespace ui {
void statistics(lv_draw_ctx_t *c, const Snapshot &d, int rangeKm, uint32_t color) {
  int e = 0, low = 0, mid = 0, high = 0;
  float max = 0;
  for (size_t i = 0; i < d.count; i++) {
    auto &a = d.planes[i];
    e += emergency(a);
    if (a.altitude < 10000)
      low++;
    else if (a.altitude < 25000)
      mid++;
    else
      high++;
    if (std::isfinite(a.altitude) && a.altitude > max)
      max = a.altitude;
  }
  text(c, 80, 92, 306, "ESTATÍSTICAS", color, 24, LV_TEXT_ALIGN_CENTER);
  char b[220];
  snprintf(b, sizeof(b),
           "%u aeronaves em %d km\n\nBaixas/médias/altas: %d / %d / %d\n\nMaior altitude: "
           "%.0f m / %.0f ft\n\nEmergências: %d\n\nFonte: %s",
           unsigned(d.count), rangeKm, low, mid, high, max * .3048f, max, e, d.provider);
  text(c, 90, 145, 290, b, color, 14);
}
void details(lv_draw_ctx_t *c, const Aircraft &a, const Route &r, uint32_t color) {
  const char *name = a.callsign[0] ? a.callsign : a.hex;
  text(c, 55, 39, 356, name, color, 24, LV_TEXT_ALIGN_CENTER);

  char badge[12]{};
  if (a.callsign[0])
    snprintf(badge, sizeof(badge), "%.3s", a.callsign);
  else
    snprintf(badge, sizeof(badge), "AIR");
  circle(c, 233, 98, 32, color, 4);
  text(c, 201, 86, 64, badge, color, 18, LV_TEXT_ALIGN_CENTER);

  float remainingKm = NAN, remainingHours = NAN, flightHours = NAN;
  char arrival[16] = "--:--", duration[24] = "--";
  if (std::isfinite(r.destinationLat) && std::isfinite(r.destinationLon) &&
      std::isfinite(a.speed) && a.speed > 40) {
    remainingKm = distanceKm(a.lat, a.lon, r.destinationLat, r.destinationLon);
    remainingHours = remainingKm / (a.speed * 1.852f);
    time_t eta = time(nullptr) + time_t(remainingHours * 3600);
    tm local{};
    localtime_r(&eta, &local);
    strftime(arrival, sizeof(arrival), "%H:%M", &local);
    snprintf(duration, sizeof(duration), "%dh%02d", int(remainingHours),
             int(remainingHours * 60) % 60);
    if (std::isfinite(r.originLat) && std::isfinite(r.originLon)) {
      flightHours = distanceKm(r.originLat, r.originLon, r.destinationLat, r.destinationLon) /
                    (a.speed * 1.852f);
      snprintf(duration, sizeof(duration), "%dh%02d", int(flightHours), int(flightHours * 60) % 60);
    }
  }
  char routeLine[120];
  snprintf(routeLine, sizeof(routeLine), "%s  >  %s%s%s", r.origin[0] ? r.origin : "---",
           r.destination[0] ? r.destination : "---", r.destinationCity[0] ? "  " : "",
           r.destinationCity);
  text(c, 38, 140, 390, routeLine, 0xffe69a, 18, LV_TEXT_ALIGN_CENTER);

  char weather[20] = "consultando";
  if (std::isfinite(r.temperature))
    snprintf(weather, sizeof(weather), "%.1f C", r.temperature);
  char b[560];
  snprintf(b, sizeof(b),
           "%s  %s\nAltitude  %.0f m / %.0f ft\nVelocidade  %.0f km/h / %.0f kt\n"
           "Distância  %.1f km / %.1f NM\nRumo  %.0f°   Squawk  %s\nChegada aprox.  %s\n"
           "Tempo de voo aprox.  %s\nClima destino  %s",
           a.registration[0] ? a.registration : "Sem matrícula",
           r.type[0] ? r.type : (a.type[0] ? a.type : "Tipo consultando"), a.altitude * .3048f,
           a.altitude, a.speed * 1.852f, a.speed, a.distance, a.distance / 1.852f, a.heading,
           a.squawk[0] ? a.squawk : "--", arrival, duration, weather);
  text(c, 28, 174, 410, b, emergency(a) ? 0xff7777 : color, 18, LV_TEXT_ALIGN_CENTER);
  text(c, 85, 434, 296, "Toque para voltar", 0xb8c9be, 12, LV_TEXT_ALIGN_CENTER);
}
} // namespace ui
