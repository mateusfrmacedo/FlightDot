#include "adsb_client.h"
#include "../core/parser.h"
#include "../core/storage.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include <time.h>
namespace adsb {
// This Arduino SDK builds mbedTLS with internal-only allocation by default.
// Install compatible heap callbacks before Wi-Fi/TLS starts; large handshakes
// then use the board's PSRAM and leave the display DMA buffers in internal RAM.
static void *tlsCalloc(size_t count, size_t size) {
  if (size && count > SIZE_MAX / size)
    return nullptr;
  void *p = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return p ? p : heap_caps_calloc(count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
void initMemory() {
  int result = mbedtls_platform_set_calloc_free(tlsCalloc, heap_caps_free);
  assert(result == 0);
}
static QueueHandle_t configs, snapshots, requests, routes;
static SemaphoreHandle_t mutex;
static char lastError[80]{};
struct ConfigRequest {
  Settings settings;
  uint32_t generation;
  char follow[20]{};
};
static uint32_t generation = 0;
static Settings currentSettings;
static char followQuery[20]{};
extern const uint8_t caBundle[] asm("_binary_certs_x509_crt_bundle_start");
extern const uint8_t caBundleEnd[] asm("_binary_certs_x509_crt_bundle_end");
extern const char googleRoot[] asm("_binary_certs_gts_root_r4_pem_start");
static void setError(const char *e) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  snprintf(lastError, sizeof(lastError), "%s", e);
  xSemaphoreGive(mutex);
}
void error(char *b, size_t n) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  snprintf(b, n, "%s", lastError);
  xSemaphoreGive(mutex);
}
static void tls(WiFiClientSecure &c, bool google = false) {
  if (google)
    c.setCACert(googleRoot);
  else
    c.setCACertBundle(caBundle, caBundleEnd - caBundle);
  c.setHandshakeTimeout(6);
  c.setTimeout(6000);
}
static int fetch(const char *host, const Settings &cfg, Snapshot &out, const char *call = "",
                 const char *hex = "") {
  WiFiClientSecure client;
  tls(client, strcmp(host, "api.airplanes.live") == 0 || strcmp(host, "opendata.adsb.fi") == 0);
  HTTPClient http;
  char url[180];
  const char *path = strcmp(host, "opendata.adsb.fi") == 0
                         ? "https://%s/api/v3/lat/%.6f/lon/%.6f/dist/%d"
                         : "https://%s/v2/point/%.6f/%.6f/%d";
  snprintf(url, sizeof(url), path, host, cfg.lat, cfg.lon, int(ceil(cfg.rangeKm / 1.852)));
  if (*call || *hex) {
    const char *prefix = strcmp(host, "opendata.adsb.fi") == 0 ? "/api/v2/" : "/v2/";
    snprintf(url, sizeof(url), "https://%s%s%s/%s", host, prefix, *hex ? "hex" : "callsign",
             *hex ? hex : call);
  }
  Settings bounds = cfg;
  if (*call || *hex)
    bounds.rangeKm = 21000; // Global query, independent of saved scope.
  clearSnapshot(out);
  http.useHTTP10(true);
  http.setConnectTimeout(4000);
  http.setTimeout(6000);
  http.setReuse(false);
  if (!http.begin(client, url))
    return -1;
  http.setUserAgent("FlightDot/1.0 (personal educational ESP32)");
  http.addHeader("Accept", "application/json");
  const char *headers[] = {"Transfer-Encoding"};
  http.collectHeaders(headers, 1);
  int code = http.GET();
  char err[80];
  if (code == 200 && http.header("Transfer-Encoding").length() == 0) {
    auto &stream = http.getStream();
    uint32_t start = millis();
    int remaining = http.getSize();
    bool ok = parseAircraft(
        [&]() -> int {
          if (remaining == 0)
            return -1;
          while (millis() - start < 10000) {
            if (stream.available()) {
              int c = stream.read();
              if (remaining > 0)
                remaining--;
              return c;
            }
            if (!http.connected())
              return -1;
            vTaskDelay(1);
          }
          return -1;
        },
        bounds, out, err, sizeof(err));
    if (ok) {
      out.updatedMs = millis();
      snprintf(out.provider, sizeof(out.provider), "%s", host);
      setError("");
      http.end();
      return 200;
    }
    code = -2;
    setError(err);
  } else {
    snprintf(err, sizeof(err), "%s: HTTP %d", host, code);
    setError(err);
    if (code > 0) {
      char body[241]{};
      auto &stream = http.getStream();
      size_t n = stream.readBytes(body, std::min(240, std::max(0, http.getSize())));
      body[n] = 0;
      Serial.printf("[adsb] resposta: %s\n", body);
    }
    if (code == 200)
      code = -3;
  }
  Serial.printf("[adsb] %s code=%d\n", host, code);
  http.end();
  return code;
}
static bool safeToken(const char *s) {
  if (!*s)
    return false;
  for (; *s; s++)
    if (!isalnum((unsigned char)*s) && *s != '-')
      return false;
  return true;
}
static void fetchWeather(Route &r) {
  if (!std::isfinite(r.destinationLat) || !std::isfinite(r.destinationLon))
    return;
  WiFiClientSecure client;
  tls(client);
  HTTPClient http;
  char url[220];
  snprintf(
      url, sizeof(url),
      "https://api.open-meteo.com/v1/forecast?latitude=%.5f&longitude=%.5f&current=temperature_2m",
      r.destinationLat, r.destinationLon);
  http.useHTTP10(true);
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.setUserAgent("FlightDot/1.0 (personal ESP32)");
  if (http.begin(client, url) && http.GET() == 200) {
    DynamicJsonDocument weather(768), filter(192);
    filter["current"]["temperature_2m"] = true;
    if (!deserializeJson(weather, http.getStream(), DeserializationOption::Filter(filter)))
      r.temperature = weather["current"]["temperature_2m"] | NAN;
  }
  http.end();
}
static Route enrich(const Aircraft &a) {
  Route r;
  if (storage::readRoute(a.hex, a.callsign, r))
    return r;
  r = {};
  snprintf(r.hex, sizeof(r.hex), "%s", a.hex);
  snprintf(r.callsign, sizeof(r.callsign), "%s", a.callsign);
  if (!safeToken(a.hex))
    return r;
  WiFiClientSecure client;
  tls(client);
  HTTPClient http;
  String url = "https://api.adsbdb.com/v0/aircraft/" + String(a.hex);
  if (safeToken(a.callsign))
    url += "?callsign=" + String(a.callsign);
  http.useHTTP10(true);
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.setUserAgent("FlightDot/1.0");
  if (!http.begin(client, url))
    return r;
  int code = http.GET();
  if (code == 200) {
    DynamicJsonDocument doc(6144), filter(1024);
    filter["response"]["aircraft"]["type"] = true;
    filter["response"]["flightroute"]["airline"]["name"] = true;
    filter["response"]["flightroute"]["origin"]["icao_code"] = true;
    filter["response"]["flightroute"]["origin"]["latitude"] = true;
    filter["response"]["flightroute"]["origin"]["longitude"] = true;
    filter["response"]["flightroute"]["destination"]["icao_code"] = true;
    filter["response"]["flightroute"]["destination"]["name"] = true;
    filter["response"]["flightroute"]["destination"]["municipality"] = true;
    filter["response"]["flightroute"]["destination"]["latitude"] = true;
    filter["response"]["flightroute"]["destination"]["longitude"] = true;
    if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
      snprintf(r.type, sizeof(r.type), "%s", doc["response"]["aircraft"]["type"] | "");
      snprintf(r.origin, sizeof(r.origin), "%s",
               doc["response"]["flightroute"]["origin"]["icao_code"] | "");
      snprintf(r.destination, sizeof(r.destination), "%s",
               doc["response"]["flightroute"]["destination"]["icao_code"] | "");
      snprintf(r.airline, sizeof(r.airline), "%s",
               doc["response"]["flightroute"]["airline"]["name"] | "");
      snprintf(r.destinationName, sizeof(r.destinationName), "%s",
               doc["response"]["flightroute"]["destination"]["name"] | "");
      snprintf(r.destinationCity, sizeof(r.destinationCity), "%s",
               doc["response"]["flightroute"]["destination"]["municipality"] | "");
      r.originLat = doc["response"]["flightroute"]["origin"]["latitude"] | NAN;
      r.originLon = doc["response"]["flightroute"]["origin"]["longitude"] | NAN;
      r.destinationLat = doc["response"]["flightroute"]["destination"]["latitude"] | NAN;
      r.destinationLon = doc["response"]["flightroute"]["destination"]["longitude"] | NAN;
      r.found = r.type[0] || r.origin[0];
      r.fetchedAt = time(nullptr);
    }
  } else if (code == 404) {
    r.fetchedAt = time(nullptr);
    storage::writeRoute(r);
  }
  http.end();
  if (r.found) {
    fetchWeather(r);
    storage::writeRoute(r);
  }
  return r;
}
// Resolve airline ticket identifiers (IATA) to the broadcast callsign once per search.
static void resolveCallsign(const char *query, char *out, size_t size) {
  snprintf(out, size, "%s", query);
  // ICAO callsigns already have a three-letter airline prefix.
  if (strlen(query) >= 4 && isalpha(query[0]) && isalpha(query[1]) && isalpha(query[2]))
    return;
  WiFiClientSecure client;
  tls(client);
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(4000);
  http.setTimeout(5000);
  http.setUserAgent("FlightDot/1.0");
  if (!http.begin(client, String("https://api.adsbdb.com/v0/callsign/") + query))
    return;
  if (http.GET() == 200) {
    DynamicJsonDocument doc(1024), filter(256);
    filter["response"]["flightroute"]["callsign_icao"] = true;
    if (!deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter))) {
      char normalized[20];
      const char *value = doc["response"]["flightroute"]["callsign_icao"] | "";
      if (*value && normalizeFlight(value, normalized, sizeof(normalized)))
        snprintf(out, size, "%s", normalized);
    }
  }
  http.end();
}
static void worker(void *) {
  ConfigRequest request;
  xQueueReceive(configs, &request, portMAX_DELAY);
  Settings cfg = request.settings;
  static Snapshot next;
  char resolved[80]{}, lockedHex[12]{};
  bool resolve = request.follow[0];
  uint32_t due = 0, cooldown[3] = {0, 0, 0}, routeDue = 0;
  int host = 0;
  unsigned failures = 0;
  const char *hosts[] = {"api.airplanes.live", "api.adsb.lol", "opendata.adsb.fi"};
  for (;;) {
    ConfigRequest changed;
    if (xQueueReceive(configs, &changed, 0)) {
      if (strcmp(request.follow, changed.follow)) {
        resolved[0] = lockedHex[0] = 0;
        resolve = changed.follow[0];
      }
      request = changed;
      cfg = changed.settings;
      due = millis() + 5000;
    }
    uint32_t now = millis();
    if (WiFi.status() == WL_CONNECTED && time(nullptr) > 1700000000) {
      if (int32_t(now - due) >= 0) {
        for (int tries = 0; tries < 3 && int32_t(now - cooldown[host]) < 0; ++tries)
          host = (host + 1) % 3;
        if (int32_t(now - cooldown[host]) >= 0) {
          if (resolve) {
            char primary[20];
            resolveCallsign(request.follow, primary, sizeof(primary));
            snprintf(resolved, sizeof(resolved), "%s", primary);
            // LATAM ticket numbers use LA across operators. The route database
            // may return LAN even while the active Brazilian aircraft emits TAM.
            if (!strncmp(request.follow, "LA", 2) && isdigit(request.follow[2])) {
              snprintf(resolved, sizeof(resolved), "TAM%s,LAN%s", request.follow + 2,
                       request.follow + 2);
              if (strncmp(primary, "TAM", 3) && strncmp(primary, "LAN", 3)) {
                size_t used = strlen(resolved);
                snprintf(resolved + used, sizeof(resolved) - used, ",%s", primary);
              }
            }
            resolve = false;
          }
          int code = fetch(hosts[host], cfg, next, resolved, lockedHex);
          now = millis();
          if (code == 200) {
            next.generation = request.generation;
            failures = 0;
            if (request.follow[0]) {
              if (next.count == 1 && (!lockedHex[0] || !strcmp(lockedHex, next.planes[0].hex))) {
                auto &plane = next.planes[0];
                snprintf(lockedHex, sizeof(lockedHex), "%s", plane.hex);
                next.following = true;
                next.centerLat = plane.lat;
                next.centerLon = plane.lon;
                plane.distance = plane.bearing = 0;
                xQueueOverwrite(snapshots, &next);
              } else {
                setError(next.count > 1 ? "Voo ambiguo: use callsign exato"
                                        : "Voo sem posicao ADS-B recente");
                // Keep the last position with its original timestamp so it becomes stale.
                host = (host + 1) % 3;
              }
            } else
              xQueueOverwrite(snapshots, &next);
            due = now + 5000;
          } else {
            failures++;
            cooldown[host] = now + ((code == 403 || code == 429) ? 600000 : 15000);
            host = (host + 1) % 3;
            due = now + std::min(60000u, 5000u * std::min(failures, 12u));
          }
        } else
          due = now + 5000;
      }
      Aircraft a;
      if (int32_t(now - routeDue) >= 0 && xQueueReceive(requests, &a, 0)) {
        Route r = enrich(a);
        xQueueOverwrite(routes, &r);
        routeDue = millis() + 5000;
      }
    } else if (WiFi.status() == WL_CONNECTED)
      setError("Aguardando horario NTP para HTTPS");
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void begin(const Settings &c) {
  configs = xQueueCreate(1, sizeof(ConfigRequest));
  snapshots = xQueueCreate(1, sizeof(Snapshot));
  requests = xQueueCreate(1, sizeof(Aircraft));
  routes = xQueueCreate(1, sizeof(Route));
  mutex = xSemaphoreCreateMutex();
  assert(configs && snapshots && requests && routes && mutex);
  configure(c);
  BaseType_t ok = xTaskCreatePinnedToCore(worker, "adsb", 24576, nullptr, 1, nullptr, 0);
  assert(ok == pdPASS);
}
void configure(const Settings &c) {
  currentSettings = c;
  ConfigRequest r{c, ++generation};
  snprintf(r.follow, sizeof(r.follow), "%s", followQuery);
  xQueueOverwrite(configs, &r);
  xQueueReset(snapshots);
}
bool follow(const char *query) {
  char value[20];
  if (!normalizeFlight(query, value, sizeof(value)))
    return false;
  snprintf(followQuery, sizeof(followQuery), "%s", value);
  setError("");
  configure(currentSettings);
  return true;
}
void following(char *out, size_t size) {
  snprintf(out, size, "%s", followQuery);
}
bool snapshot(Snapshot &s) {
  return xQueueReceive(snapshots, &s, 0) == pdTRUE && s.generation == generation;
}
void requestRoute(const Aircraft &a) {
  xQueueOverwrite(requests, &a);
}
bool route(Route &r) {
  return xQueueReceive(routes, &r, 0) == pdTRUE;
}
} // namespace adsb
