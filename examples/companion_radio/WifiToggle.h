#pragma once

// Signal word for the WiFi page: good from -67 dBm up, fair from -75 to -68 dBm, weak below.
inline const char* wifiSignalWord(int rssi) { return rssi >= -67 ? "good" : rssi >= -75 ? "fair" : "weak"; }

// Companion WiFi page logic (our addition). No Arduino or WiFi code here, so the host tests in
// test/test_ours_wifi_toggle can drive it. WifiControl.cpp supplies the real steps.
// Turning WiFi on always re-reads the saved settings: a network cleared by the app keeps WiFi off,
// and a changed network is used at once. Turning WiFi off never changes the saved network.
class WifiToggle {
public:
  struct Ops {
    virtual void savePref(bool on) = 0;  // NodePrefs::wifi_enabled, then savePrefs()
    virtual bool loadSsid() = 0;         // re-read the saved SSID and password; false if none
    virtual void serverBegin() = 0;      // wifi_interface.begin(TCP_PORT), once per boot
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
      } else {
        if (!_server_up) { ops.serverBegin(); _server_up = true; }
        ops.attach();
      }
      ops.linkStart();
      return true;
    }
    if (running) { ops.linkStop(); ops.detach(); }
    return false;
  }

private:
  bool _noted = false;
  bool _server_up = false;
};
