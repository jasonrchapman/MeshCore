#include "WifiLink.h"

#if defined(ENABLE_WIFI_INTERFACE) && defined(ESP32)
#include <Arduino.h>
#include <WiFi.h>
#include <helpers/wifi/SerialWifiInterface.h>
#include "MyMesh.h"
#include "target.h"

// upstream globals in main.cpp
extern SerialWifiInterface wifi_interface;
extern bool wifi_enabled;
extern char wifi_ssid[33];
extern char wifi_pwd[64];

void wifiControlLoop();   // WifiControl.cpp; no include, because it includes WifiLink.h

static WifiRetry wl_retry;
static bool wl_init = false;
static bool wl_boot_started = false;
static volatile bool wl_ev_connected = false;
static volatile bool wl_ev_disconnected = false;
static volatile uint8_t wl_ev_reason = 0;

// Disconnect reasons that mean a wrong password (ESP-IDF wifi_err_reason_t).
static bool isWrongPassword(uint8_t r) {
  return r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_AUTH_FAIL ||
         r == WIFI_REASON_HANDSHAKE_TIMEOUT;
}

static void radioOff() {
  wifi_interface.stopClient();   // a write to a dead client can block loop() for seconds
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  board.setInhibitSleep(false);
}

static void act(WifiRetry::Action a) {
  if (a == WifiRetry::START_TRY) {
    board.setInhibitSleep(true);   // as setup() does while WiFi is active
    WiFi.setAutoReconnect(false);  // our rules decide when to try again
    WiFi.begin(wifi_ssid, wifi_pwd);
  } else if (a == WifiRetry::STOP_RADIO) {
    radioOff();
    if (wl_retry.state() == WifiRetry::GAVE_UP) wifi_enabled = false;   // off until the page or a boot
  }
}

void wifiLinkEarlyInit() {
  static bool registered = false;
  if (registered) return;
  registered = true;
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {   // runs on the Arduino event task
    if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      wl_ev_connected = true;
    } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      uint8_t reason = info.wifi_sta_disconnected.reason;
      if (reason == WIFI_REASON_ASSOC_LEAVE) return;   // our own disconnect, as the framework also ignores it
      wl_ev_reason = reason;
      wl_ev_disconnected = true;
    } else if (event == ARDUINO_EVENT_WIFI_STA_LOST_IP) {   // lease lost: a drop that is not a wrong password
      wl_ev_reason = 0;
      wl_ev_disconnected = true;
    }
  });
}

static void initOnce() {
  if (wl_init) return;
  wl_init = true;
  wifiLinkEarlyInit();                  // normally done by setup() already
  wl_boot_started = wifi_enabled && wifi_ssid[0];
  if (wl_boot_started) {                // setup() already began the first try
    WiFi.setAutoReconnect(false);
    wl_retry.start(millis());           // its START_TRY is the try that setup() began
  }
}

#ifdef OURS_LOOP_PROBE
// Test builds only (the isolation test): every 10 s, one line on Serial with the longest loop()
// gap, free memory, LoRa packets received, the WiFi state, the last disconnect reason and the battery.
static void probe(uint32_t now) {
  static uint32_t last = 0, max_gap = 0, next = 10000;
  if (last != 0 && now - last > max_gap) max_gap = now - last;
  last = now;
  if ((int32_t)(now - next) < 0) return;
  next = now + 10000;
  Serial.printf("PROBE t=%lu gap_max=%lu heap=%lu heap_min=%lu rx=%lu wifi=%d reason=%u batt=%u\n",
                (unsigned long)now, (unsigned long)max_gap, (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap(),
                (unsigned long)(the_mesh.getNumRecvFlood() + the_mesh.getNumRecvDirect()),
                (int)wl_retry.state(), (unsigned)wl_ev_reason, (unsigned)board.getBattMilliVolts());
  max_gap = 0;
}
#endif

void wifiLinkLoop() {
  initOnce();
  uint32_t now = millis();
  if (wl_ev_connected) {
    wl_ev_connected = false;
    if (WiFi.status() == WL_CONNECTED) wl_retry.onConnected(now);   // still joined when we look
  }
  if (wl_ev_disconnected) {
    wl_ev_disconnected = false;
    act(wl_retry.onDisconnected(now, isWrongPassword(wl_ev_reason)));
  }
  act(wl_retry.poll(now));
  // The framework retries the first failed join of a boot by itself, on the event task, and that can
  // restart the radio after our radioOff(). So make sure it is really off while the rules say off.
  WifiRetry::State st = wl_retry.state();
  if ((st == WifiRetry::WAITING || st == WifiRetry::GAVE_UP || st == WifiRetry::OFF) && WiFi.getMode() != WIFI_OFF)
    radioOff();
#ifdef OURS_LOOP_PROBE
  probe(now);
#endif
  wifiControlLoop();
}

void wifiLinkStart() {
  initOnce();
  wifi_enabled = true;
  act(wl_retry.start(millis()));
}

void wifiLinkStop() {
  initOnce();
  act(wl_retry.stop());
  wifi_enabled = false;
}

bool wifiLinkStartedAtBoot() { initOnce(); return wl_boot_started; }
WifiRetry::State wifiLinkState() { initOnce(); return wl_retry.state(); }
WifiRetry::Reason wifiLinkReason() { initOnce(); return wl_retry.reason(); }
uint32_t wifiLinkSecondsToNextTry() { initOnce(); return wl_retry.secondsToNextTry(millis()); }
#endif
