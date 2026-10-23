#pragma once
#include <string.h>

// Signal word for the WiFi page: good from -67 dBm up, fair from -75 to -68 dBm, weak below.
inline const char* wifiSignalWord(int rssi) { return rssi >= -67 ? "good" : rssi >= -75 ? "fair" : "weak"; }

// True when the saved network and password BOTH differ from the running pair (user rule, 2026-10-23).
// A one-sided change waits for a reboot or a page off and on, because it can be the first of the
// app's two commands (set wifi.ssid, set wifi.pwd). An empty saved SSID is the cleared case, not this.
inline bool wifiPairChanged(const char* saved_ssid, const char* saved_pwd, const char* run_ssid, const char* run_pwd) {
  return saved_ssid[0] != 0 && strcmp(saved_ssid, run_ssid) != 0 && strcmp(saved_pwd, run_pwd) != 0;
}

// Companion WiFi page logic (our addition). No Arduino or WiFi code here, so the host tests in
// test/test_ours_wifi_toggle can drive it. WifiControl.cpp supplies the real steps.
// Turning WiFi on always re-reads the saved settings: a network cleared by the app keeps WiFi off,
// and a changed network is used at once. Turning WiFi off never changes the saved network.
class WifiToggle {
public:
  struct Ops {
    virtual void savePref(bool on) = 0;  // NodePrefs::wifi_enabled, then savePrefs()
    virtual bool loadSsid() = 0;         // re-read the saved SSID and password; false if none
    virtual void serverBegin() = 0;      // wifi_interface.begin(TCP_PORT), once per boot; after linkStart(): the ESP32 TCP/IP stack starts in WiFi.begin()
    virtual void attach() = 0;           // addInterface(), enable()
    virtual void detach() = 0;           // stopClient(), disable(), removeInterface()
    virtual void linkStart() = 0;        // WifiRetry::start(): radio on, first try
    virtual void linkStop() = 0;         // WifiRetry::stop(): radio off
    virtual ~Ops() {}
  };

  // Once, before the first set(): true if setup() started WiFi at boot (the server is up).
  void noteBoot(bool started_at_boot) {
    if (_noted) return;
    _noted = true;
    _server_up = started_at_boot;
  }

  // Saves the choice, then starts or stops WiFi. running: WiFi is on now (trying or joined).
  // live: false on chips where the change waits for a reboot. Returns true if WiFi is on afterwards.
  bool set(bool on, bool running, bool live, Ops& ops) {
    ops.savePref(on);
    if (!live) return running;
    if (on) {
      if (!ops.loadSsid()) {   // no saved network: WiFi stays off
        if (running) { ops.linkStop(); ops.detach(); }
        return false;
      }
      if (running) {
        ops.linkStop();        // start again with the settings just read
        ops.linkStart();
      } else {
        ops.linkStart();       // radio first, because on ESP32 the TCP/IP stack only exists after WiFi.begin()
        if (!_server_up) { ops.serverBegin(); _server_up = true; }
        ops.attach();
      }
      return true;
    }
    if (running) { ops.linkStop(); ops.detach(); }
    return false;
  }

private:
  bool _noted = false;
  bool _server_up = false;
};
