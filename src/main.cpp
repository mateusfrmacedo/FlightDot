#include "hardware/display.h"
#include <Arduino.h>
#include <lvgl.h>
#ifndef PLANO_DISPLAY_TEST
#include "core/storage.h"
#include "hardware/rtc.h"
#include "net/adsb_client.h"
#include "net/web_config.h"
#include "net/wifi_manager.h"
#include "ui/ui.h"
#include <WiFi.h>
#include <esp_sntp.h>
static Settings settings;
static Snapshot snapshot;
static Status status;
static volatile bool clockSynced = false;
static void applySettings() {
  setenv("TZ", settings.timezone, 1);
  tzset();
  display::brightness(settings.brightness);
  ui::setSettings(settings);
#ifndef PLANO_MOCK
  adsb::configure(settings);
#endif
  clearSnapshot(snapshot);
}
#endif
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("[boot] flash=%u psram=%u\n", ESP.getFlashChipSize(), ESP.getPsramSize());
  display::begin();
#ifdef PLANO_DISPLAY_TEST
  display::testScreen();
#else
  settings = storage::load();
  rtc::begin();
  ui::begin(settings);
#ifndef PLANO_MOCK
  adsb::initMemory();
#endif
  wifiManager::begin();
  webConfig::begin(settings);
  sntp_set_time_sync_notification_cb([](struct timeval *) { clockSynced = true; });
  configTzTime(settings.timezone, "pool.ntp.org", "time.google.com");
  display::brightness(settings.brightness);
#ifndef PLANO_MOCK
  adsb::begin(settings);
#endif
  Serial.println("[boot] Radar pronto");
#endif
}
void loop() {
  static uint32_t last = millis();
  uint32_t now = millis();
  lv_tick_inc(now - last);
  last = now;
#ifndef PLANO_DISPLAY_TEST
  wifiManager::tick();
  webConfig::tick();
  if (webConfig::changed())
    applySettings();
  if (clockSynced) {
    clockSynced = false;
    rtc::sync();
  }
#ifdef PLANO_MOCK
  static uint32_t mockAt = 0;
  if (!snapshot.valid || now - mockAt >= 5000) {
    mockAt = now;
    mockSnapshot(snapshot, settings, now);
    ui::update(snapshot);
  }
#else
  char follow[20];
  adsb::following(follow, sizeof(follow));
  if (strcmp(status.follow, follow)) {
    snprintf(status.follow, sizeof(status.follow), "%s", follow);
    clearSnapshot(snapshot);
    ui::update(snapshot);
    ui::setStatus(status);
    webConfig::status(snapshot, status);
  }
  static Snapshot incoming;
  if (adsb::snapshot(incoming)) {
    snapshot = incoming;
    ui::update(snapshot);
    Serial.printf("[adsb] %u voos via %s heap=%u\n", unsigned(snapshot.count), snapshot.provider,
                  ESP.getFreeHeap());
  }
  Route r;
  if (adsb::route(r))
    ui::setRoute(r);
#endif
  Settings requested;
  Aircraft a;
  auto action = ui::takeAction(requested, a);
  if (action == ui::Action::SettingsChanged) {
    bool geographic = requested.rangeKm != settings.rangeKm || requested.lat != settings.lat ||
                      requested.lon != settings.lon;
    settings = requested;
    storage::save(settings);
    if (geographic) {
      applySettings();
      ui::update(snapshot);
    } else
      display::brightness(settings.brightness);
  }
#ifndef PLANO_MOCK
  if (action == ui::Action::RouteRequested)
    adsb::requestRoute(a);
#endif
  static uint32_t hud = 0;
  static uint32_t lastFrames = 0;
  if (now - hud >= 1000) {
    uint32_t frames = display::frameCount();
    status.fps = (frames - lastFrames) * 1000.f / (now - hud);
    lastFrames = frames;
    hud = now;
    status.wifi = WiFi.status() == WL_CONNECTED;
    status.setup = wifiManager::portal();
    snprintf(status.ip, sizeof(status.ip), "%s", WiFi.localIP().toString().c_str());
    time_t epoch = time(nullptr);
    tm t;
    localtime_r(&epoch, &t);
    if (epoch > 1700000000) {
      strftime(status.clock, sizeof(status.clock), "%H:%M:%S", &t);
      strftime(status.date, sizeof(status.date), "%d/%m", &t);
    }
    status.voltage = rtc::voltage();
    static uint32_t performanceLog = 0;
    if (now - performanceLog >= 10000) {
      performanceLog = now;
      Serial.printf("[ui] fps=%.1f heap=%u\n", status.fps, ESP.getFreeHeap());
    }
#ifndef PLANO_MOCK
    adsb::error(status.error, sizeof(status.error));
    adsb::following(status.follow, sizeof(status.follow));
#endif
    ui::setStatus(status);
    webConfig::status(snapshot, status);
    display::brightness(settings.brightness);
  }
  ui::tick(now);
#endif
  lv_timer_handler();
  delay(5);
}
