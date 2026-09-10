#include "web_config.h"
#include "../core/storage.h"
#include "adsb_client.h"
#include "web_page.h"
#include "wifi_manager.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <cstdlib>
namespace webConfig {
static WebServer server(80);
static Settings *config;
static bool dirty = false, updating = false, otaFailed = false;
static String token, password;
static uint32_t reboot = 0;
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
  Preferences p;
  p.begin("plano", false);
  password = p.getString("otaPass", "");
  if (password.isEmpty()) {
    password = token.substring(0, 16);
    p.putString("otaPass", password);
  }
  p.end();
  Serial.printf("[web] OTA usuario=admin senha=%s\n", password.c_str());
  const char *headers[] = {"X-Radar-Token", "Authorization"};
  server.collectHeaders(headers, 2);
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
    d["theme"] = config->theme;
    d["brightness"] = config->brightness;
    d["staleSeconds"] = config->staleSeconds;
    d["timezone"] = config->timezone;
    d["centerName"] = config->centerName;
    d["sweep"] = config->sweep;
    d["autoDim"] = config->autoDim;
    d["token"] = token;
    d["portal"] = wifiManager::portal();
    json(d);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!authorized())
      return;
    Settings s = *config;
    bool ok = number("lat", s.lat) && number("lon", s.lon) && integer("rangeKm", s.rangeKm) &&
              integer("theme", s.theme) && integer("brightness", s.brightness) &&
              integer("staleSeconds", s.staleSeconds);
    String z = server.arg("timezone");
    if (z.isEmpty() || z.length() >= sizeof(s.timezone))
      ok = false;
    for (char c : z)
      if (!isalnum((unsigned char)c) && String("<>+-:,./").indexOf(c) < 0)
        ok = false;
    snprintf(s.timezone, sizeof(s.timezone), "%s", z.c_str());
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
      server.send(400, "text/plain", "Valores invalidos. Verifique coordenadas, alcance e fuso.");
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
  server.on("/api/reset-wifi", HTTP_POST, [] {
    if (!authorized())
      return;
    server.send(200, "text/plain", "Wi-Fi esquecido. Conecte ao FlightDot-Setup.");
    wifiManager::reset();
  });
  server.on(
      "/update", HTTP_POST,
      [] {
        if (!server.authenticate("admin", password.c_str())) {
          server.requestAuthentication();
          return;
        }
        if (!authorized())
          return;
        if (!updating || otaFailed || Update.hasError()) {
          server.send(400, "text/plain", "Falha na atualizacao. Firmware anterior preservado.");
          updating = false;
          return;
        }
        server.send(200, "text/plain", "Atualizacao concluida. Reiniciando...");
        reboot = millis() + 1500;
      },
      [] {
        auto &u = server.upload();
        if (u.status == UPLOAD_FILE_START) {
          updating = false;
          otaFailed = false;
          if (!server.authenticate("admin", password.c_str()) || server.arg("token") != token) {
            otaFailed = true;
            return;
          }
          updating = Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
          otaFailed = !updating;
        } else if (u.status == UPLOAD_FILE_WRITE) {
          if (updating && !otaFailed && Update.write(u.buf, u.currentSize) != u.currentSize) {
            otaFailed = true;
            Update.abort();
          }
        } else if (u.status == UPLOAD_FILE_END) {
          if (updating && !otaFailed && !Update.end(true))
            otaFailed = true;
        } else if (u.status == UPLOAD_FILE_ABORTED) {
          Update.abort();
          updating = false;
          otaFailed = true;
        }
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
  if (reboot && int32_t(millis() - reboot) >= 0)
    ESP.restart();
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
