#pragma once

// Companion WiFi page support (our addition). Upstream starts WiFi once, in setup().
// These functions save wifi.enabled and start or stop WiFi while the firmware runs, with the
// same steps that setup() uses. Recheck them against main.cpp at each upstream release.
#ifdef ENABLE_WIFI_INTERFACE
#include <stddef.h>

bool wifiHasSsid();           // a saved (or built-in) SSID exists, read from the settings now
bool wifiIsOn();              // ESP32: trying or joined now. Other chips: saved as on, with an SSID
bool wifiIsConnected();       // joined, so the IP address is real
bool wifiCanApplyLive();      // false where the change needs a reboot (non-ESP32)
void wifiSetEnabled(bool on); // save wifi.enabled, then start or stop WiFi where possible

// The three text lines of the WiFi page (layout A), each at most n - 1 characters.
void wifiPageLines(char* l1, char* l2, char* l3, size_t n);
bool wifiGaveUp();            // retries ended: the bottom line says "retry"
#endif
