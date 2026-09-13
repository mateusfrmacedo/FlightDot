#include "web_config.h"
#include "../core/storage.h"
#include "adsb_client.h"
#include "web_page.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cstdlib>
namespace webConfig {
static WebServer server(80);
static Settings *config;
static bool dirty = false;
static String token;
static size_t flights = 0;
static uint32_t updated = 0;
static bool valid = false;
static char provider[24]{};
static Status state;
static Aircraft tracked;
static bool followingSnapshot = false;
extern const uint8_t airportsStart[] asm("_binary_data_airports_json_gz_start");
extern const uint8_t airportsEnd[] asm("_binary_data_airports_json_gz_end");
static bool authorized() {
  if (server.header("X-Radar-Token") != token && server.arg("token") != token) {
    server.send(403, "text/plain", "Token invalido. Recarregue a pagina.");
    return false;
  }
  return true;
}
static void json(JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}
static bool number(const char *name, double &out) {
  String s = server.arg(name);
  if (!s.length())
    return false;
  char *end = nullptr;
  out = strtod(s.c_str(), &end);
  return end && !*end && std::isfinite(out);
}
static bool integer(const char *name, int &out) {
  double v;
  if (!number(name, v) || v != floor(v) || fabs(v) > 100000)
    return false;
  out = int(v);
  return true;
}
void begin(Settings &s) {
  config = &s;
  char b[33];
  snprintf(b, sizeof(b), "%08lx%08lx%08lx%08lx", (unsigned long)esp_random(),
           (unsigned long)esp_random(), (unsigned long)esp_random(), (unsigned long)esp_random());
  token = b;
  const char *headers[] = {"X-Radar-Token"};
  server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/airports", HTTP_GET, [] {
    server.sendHeader("Content-Encoding", "gzip");
    server.sendHeader("Cache-Control", "public, max-age=86400");
    server.send_P(200, "application/json; charset=utf-8", (const char *)airportsStart,
                  airportsEnd - airportsStart);
  });
  server.on("/api/follow", HTTP_POST, [] {
    if (!authorized())
      return;
#ifdef PLANO_MOCK
    server.send(503, "text/plain", "Busca de voo requer firmware em modo real.");
#else
    if (!server.hasArg("query") || server.arg("query").length() > 32 ||
        !adsb::follow(server.arg("query").c_str())) {
      server.send(400, "text/plain; charset=utf-8", "Informe companhia e número: LA3030, G31600 ou TAM3030.");
      return;
    }
    server.send(200, "text/plain; charset=utf-8", server.arg("query").length() ?
                "Buscando voo. O radar acompanhará a aeronave quando houver posição disponível." :
                "Acompanhamento encerrado. Voltando ao centro salvo.");
#endif
  });
  server.on("/api/config", HTTP_GET, [] {
    StaticJsonDocument<1024> d;
    d["lat"] = config->lat;
    d["lon"] = config->lon;
    d["rangeKm"] = config->rangeKm;
    d["brightness"] = config->brightness;
    d["centerName"] = config->centerName;
    d["sweep"] = config->sweep;
    d["autoDim"] = config->autoDim;
    d["token"] = token;
    d["portal"] = wifiManager::portal();
    d["ssid"] = WiFi.SSID();
    if (WiFi.status() == WL_CONNECTED)
      d["rssi"] = WiFi.RSSI();
    else
      d["rssi"] = nullptr;
    json(d);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!authorized())
      return;
    Settings s = *config;
    bool ok = number("lat", s.lat) && number("lon", s.lon) && integer("rangeKm", s.rangeKm) &&
              integer("brightness", s.brightness);
    s.theme = 0;
    s.staleSeconds = 20;
    snprintf(s.timezone, sizeof(s.timezone), "%s", "<-03>3");
    String center = server.arg("centerName");
    if (center.isEmpty() || center.length() >= sizeof(s.centerName))
      ok = false;
    for (char c : center)
      if ((unsigned char)c < 32 || c == '<' || c == '>')
        ok = false;
    snprintf(s.centerName, sizeof(s.centerName), "%s", center.c_str());
    s.sweep = server.arg("sweep") == "1";
    s.autoDim = false;
    if (!ok || !validSettings(s)) {
      server.send(400, "text/plain", "Valores invalidos. Verifique coordenadas e alcance.");
      return;
    }
    if (!storage::save(s)) {
      server.send(500, "text/plain", "Falha ao salvar NVS");
      return;
    }
#ifndef PLANO_MOCK
    if (s.lat != config->lat || s.lon != config->lon)
      adsb::follow("");
#endif
    *config = s;
    dirty = true;
    server.send(200, "text/plain; charset=utf-8", "Configurações salvas.");
  });
  server.on("/api/status", HTTP_GET, [] {
    StaticJsonDocument<1536> d;
    d["follow"] = state.follow;
    if (state.follow[0] && followingSnapshot) {
      d["target"]["callsign"] = tracked.callsign;
      d["target"]["hex"] = tracked.hex;
      d["target"]["lat"] = tracked.lat;
      d["target"]["lon"] = tracked.lon;
    }
    d["wifi"] = state.wifi;
    d["ssid"] = WiFi.SSID();
    if (state.wifi)
      d["rssi"] = WiFi.RSSI();
    else
      d["rssi"] = nullptr;
    d["count"] = flights;
    d["provider"] = provider;
    d["error"] = state.error;
    if (valid)
      d["ageSeconds"] = (millis() - updated) / 1000;
    else
      d["ageSeconds"] = nullptr;
    d["freeHeap"] = ESP.getFreeHeap();
    d["fps"] = state.fps;
    d["uptimeSeconds"] = millis() / 1000;
    json(d);
  });
  server.on("/api/wifi", HTTP_POST, [] {
    if (!authorized())
      return;
    String ssid = server.arg("ssid"), pass = server.arg("password");
    if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 63 ||
        (pass.length() > 0 && pass.length() < 8)) {
      server.send(400, "text/plain", "SSID ou senha invalidos");
      return;
    }
    server.send(200, "text/plain",
                "Conectando. Volte para sua rede e abra http://flightdot.local/");
    wifiManager::connect(ssid.c_str(), pass.c_str());
  });
  server.on("/api/wifi/scan", HTTP_POST, [] {
    if (!authorized())
      return;
    int result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) {
      server.send(202, "application/json", "{\"scanning\":true}");
      return;
    }
    if (result < 0) {
      WiFi.scanNetworks(true, true); // asynchronous: keep the settings page responsive
      server.send(202, "application/json", "{\"scanning\":true}");
      return;
    }
    StaticJsonDocument<6144> d;
    d["scanning"] = false;
    auto networks = d.createNestedArray("networks");
    for (int i = 0; i < result; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty())
        continue;
      auto network = networks.createNestedObject();
      network["ssid"] = ssid;
      network["rssi"] = WiFi.RSSI(i);
      network["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
    json(d);
  });
  server.on("/api/reset-wifi", HTTP_POST, [] {
    if (!authorized())
      return;
    server.send(200, "text/plain", "Wi-Fi esquecido. Conecte ao FlightDot-Setup.");
    wifiManager::reset();
  });
  server.onNotFound([] {
    if (wifiManager::portal()) {
      server.sendHeader("Location", "http://192.168.4.1/");
      server.send(302, "text/plain", "");
    } else
      server.send(404, "text/plain", "Nao encontrado");
  });
  server.begin();
}
void tick() {
  server.handleClient();
}
bool changed() {
  bool b = dirty;
  dirty = false;
  return b;
}
void status(const Snapshot &d, const Status &s) {
  flights = d.count;
  followingSnapshot = d.following && d.count;
  if (followingSnapshot)
    tracked = d.planes[0];
  updated = d.updatedMs;
  valid = d.valid;
  snprintf(provider, sizeof(provider), "%s", d.provider);
  state = s;
}
} // namespace webConfig
