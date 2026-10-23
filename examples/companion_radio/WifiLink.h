#pragma once

// Companion WiFi link with our retry rules (our addition, ESP32 only). WifiRetry.h holds the rules;
// this file turns them into WiFi calls. main.cpp's loop() calls wifiLinkLoop() on ESP32 in place of
// upstream's 10-second retry. Recheck against main.cpp's setup() at each upstream release.
#ifdef ENABLE_WIFI_INTERFACE
#include <stdint.h>
#include "WifiRetry.h"

void wifiLinkLoop();                  // events, retry rules and the test probe; call from loop()
void wifiLinkStart();                 // rules on, radio on, first try with the live wifi_ssid/wifi_pwd
void wifiLinkStop();                  // rules off, client closed, radio off
bool wifiLinkStartedAtBoot();          // setup() started WiFi (and the app server) at boot
WifiRetry::State wifiLinkState();
WifiRetry::Reason wifiLinkReason();
uint32_t wifiLinkSecondsToNextTry();
#endif
