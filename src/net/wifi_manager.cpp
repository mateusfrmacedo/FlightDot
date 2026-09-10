#include "wifi_manager.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
namespace wifiManager {
static DNSServer dns;
static bool ap = false, wasOnline = false;
static uint32_t started = 0, retry = 0;
static void setupAP() {
  if (ap)
    return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("FlightDot-Setup");
  dns.start(53, "*", WiFi.softAPIP());
  ap = true;
  Serial.println("[wifi] FlightDot-Setup / http://192.168.4.1");
}
void begin() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("flightdot");
  WiFi.setAutoReconnect(true);
  WiFi.begin();
  started = millis();
  if (WiFi.SSID().isEmpty())
    setupAP();
}
bool portal() {
  return ap;
}
void tick() {
  bool online = WiFi.status() == WL_CONNECTED;
  if (online && !wasOnline) {
    MDNS.end();
    MDNS.begin("flightdot");
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[wifi] IP=%s / http://flightdot.local\n", WiFi.localIP().toString().c_str());
    if (ap) {
      dns.stop();
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      ap = false;
    }
  }
  if (!online) {
    if (wasOnline)
      started = millis();
    if (millis() - started > 20000)
      setupAP();
    if (millis() - retry > 30000) {
      retry = millis();
      WiFi.reconnect();
    }
  }
  if (ap)
    dns.processNextRequest();
  wasOnline = online;
}
void connect(const char *ssid, const char *password) {
  WiFi.begin(ssid, password);
  started = millis();
}
void reset() {
  WiFi.disconnect(false, true);
  started = millis();
  setupAP();
}
} // namespace wifiManager
