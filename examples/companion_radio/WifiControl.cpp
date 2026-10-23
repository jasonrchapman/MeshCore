#include "WifiControl.h"

#ifdef ENABLE_WIFI_INTERFACE
#include <Arduino.h>
#include <WiFi.h>
#include <helpers/MultiSerialInterface.h>
#include <helpers/wifi/SerialWifiInterface.h>
#include "MyMesh.h"
#include "WifiLink.h"
#include "WifiToggle.h"
#include "target.h"

#ifndef TCP_PORT
  #define TCP_PORT 5000   // same default as main.cpp
#endif
#ifndef WIFI_PWD
  #define WIFI_PWD ""     // same default as main.cpp
#endif

// WiFi page texts, approved by the user on 2026-10-22
#define WIFI_TXT_TRYING   "Connecting..."
#define WIFI_TXT_GAVE_UP  "Max Attempts"
#define WIFI_TXT_OFF      "WIFI OFF"

// upstream globals in main.cpp
extern MultiSerialInterface interface_manager;
extern SerialWifiInterface wifi_interface;
extern bool wifi_enabled;
extern char wifi_ssid[33];
extern char wifi_pwd[64];

static WifiToggle wc_toggle;

static bool liveRunning() {
#if defined(ESP32)
  WifiRetry::State s = wifiLinkState();
  return wifi_enabled && wifi_ssid[0] && s != WifiRetry::OFF && s != WifiRetry::GAVE_UP;
#else
  return wifi_enabled && wifi_ssid[0];
#endif
}

static void noteBootState() {   // only the first call counts
#if defined(ESP32)
  wc_toggle.noteBoot(wifiLinkStartedAtBoot());   // a boot fact, recorded before any give-up
#else
  wc_toggle.noteBoot(liveRunning());
#endif
}

struct WifiOps : WifiToggle::Ops {
  void savePref(bool on) override {
    the_mesh.getNodePrefs()->wifi_enabled = on ? 1 : 0;
    the_mesh.savePrefs();
  }
  bool loadSsid() override {   // the saved values now, not the copy from boot
    NodePrefs* p = the_mesh.getNodePrefs();
    strncpy(wifi_ssid, p->getWifiSSID(), sizeof(wifi_ssid) - 1);
    wifi_ssid[sizeof(wifi_ssid) - 1] = 0;
    strncpy(wifi_pwd, p->wifi_ssid[0] ? p->wifi_pwd : WIFI_PWD, sizeof(wifi_pwd) - 1);
    wifi_pwd[sizeof(wifi_pwd) - 1] = 0;
    return wifi_ssid[0] != 0;
  }
  void serverBegin() override { wifi_interface.begin(TCP_PORT); }
  void attach() override {
    interface_manager.removeInterface(&wifi_interface);   // never two slots, also after a give-up
    interface_manager.addInterface(InterfaceType::WiFi, &wifi_interface);
    if (interface_manager.isEnabled()) wifi_interface.enable();   // match the other interfaces
  }
  void detach() override {
    wifi_interface.stopClient();
    wifi_interface.disable();
    interface_manager.removeInterface(&wifi_interface);
  }
#if defined(ESP32)
  void linkStart() override { wifiLinkStart(); }
  void linkStop() override { wifiLinkStop(); }
#else
  void linkStart() override {}
  void linkStop() override {}
#endif
};

bool wifiHasSsid() {
  noteBootState();
  return the_mesh.getNodePrefs()->getWifiSSID()[0] != 0;
}

bool wifiIsOn() {
  noteBootState();
#if defined(ESP32)
  return liveRunning();
#else
  return the_mesh.getNodePrefs()->wifi_enabled && wifiHasSsid();   // applies at the next boot
#endif
}

bool wifiIsConnected() {
  return liveRunning() && WiFi.status() == WL_CONNECTED;
}

bool wifiCanApplyLive() {
#if defined(ESP32)
  return true;
#else
  return false;
#endif
}

void wifiSetEnabled(bool on) {
  noteBootState();
  WifiOps ops;
  wc_toggle.set(on, liveRunning(), wifiCanApplyLive(), ops);
}

static void wifiReapply() {   // settings changed: apply them, never save the on/off choice
  noteBootState();
  WifiOps ops;
  wc_toggle.reapply(liveRunning(), wifiCanApplyLive(), ops);
}

void wifiControlLoop() {
  // the user's 2026-10-23 rule: if the saved network is cleared, WiFi stops (nothing is saved)
  if (liveRunning() && the_mesh.getNodePrefs()->getWifiSSID()[0] == 0) wifiReapply();
  NodePrefs* p = the_mesh.getNodePrefs();
  if (liveRunning() && wifiPairChanged(p->getWifiSSID(), p->wifi_ssid[0] ? p->wifi_pwd : WIFI_PWD, wifi_ssid, wifi_pwd)) {
    wifiReapply();   // both settings changed: restart with the saved pair (re-read in loadSsid)
  }
}

void wifiPageLines(char* l1, char* l2, char* l3, size_t n) {
  // the network in use while WiFi runs, else the saved one the next start uses (user rule, 2026-10-23)
  snprintf(l1, n, "%s", liveRunning() ? wifi_ssid : the_mesh.getNodePrefs()->getWifiSSID());
  l2[0] = l3[0] = 0;
#if defined(ESP32)
  WifiRetry::State s = wifiLinkState();
  if (s == WifiRetry::CONNECTED && WiFi.status() == WL_CONNECTED) {
    IPAddress ip = WiFi.localIP();
    snprintf(l2, n, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    int rssi = WiFi.RSSI();
    snprintf(l3, n, "%d dBm %s", rssi, wifiSignalWord(rssi));
  } else if (s == WifiRetry::CONNECTING || s == WifiRetry::CONNECTED) {
    snprintf(l2, n, "%s", WIFI_TXT_TRYING);
  } else if (s == WifiRetry::WAITING) {
    snprintf(l2, n, "%s", WIFI_TXT_TRYING);
    snprintf(l3, n, "next try %lus", (unsigned long)wifiLinkSecondsToNextTry());
  } else if (s == WifiRetry::GAVE_UP) {
    snprintf(l1, n, "%s", wifi_ssid);   // the reason below belongs to the network that was tried (user rule, 2026-10-23)
    snprintf(l2, n, "%s", wifiLinkReason() == WifiRetry::WRONG_PASSWORD ? "Wrong password" : "No network");
    snprintf(l3, n, "%s", WIFI_TXT_GAVE_UP);
  } else
#endif
  {
    uint8_t mac[6];
    WiFi.macAddress(mac);   // the station MAC; readable with the radio off
    snprintf(l2, n, "%s", WIFI_TXT_OFF);
    snprintf(l3, n, "MAC:%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
}

bool wifiGaveUp() {
#if defined(ESP32)
  return wifiLinkState() == WifiRetry::GAVE_UP;
#else
  return false;
#endif
}
#endif
