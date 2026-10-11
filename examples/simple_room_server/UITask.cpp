#include "UITask.h"
#include <Arduino.h>
#include <helpers/CommonCLI.h>
#include "target.h"
#include "MyMesh.h"
extern MyMesh the_mesh;

#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif

// Same math as the companion's renderBatteryIndicator().
static int battPercent(uint16_t mv) {
  int pct = ((int)mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  return pct < 0 ? 0 : (pct > 100 ? 100 : pct);
}

// Compact "ago" like the companion's Recent list: 14s / 5m / 3h / 2d.
static void formatAgo(char* buf, const char* label, uint32_t secs) {
  if (secs < 60)         sprintf(buf, "%s%lus", label, (unsigned long)secs);
  else if (secs < 3600)  sprintf(buf, "%s%lum", label, (unsigned long)(secs / 60));
  else if (secs < 86400) sprintf(buf, "%s%luh", label, (unsigned long)(secs / 3600));
  else                   sprintf(buf, "%s%lud", label, (unsigned long)(secs / 86400));
}

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      20000  // 20 seconds
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds

// 'meshcore', 128x13px
static const uint8_t meshcore_logo [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe, 
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc, 
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00, 
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00, 
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0, 
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00, 
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00, 
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8, 
};

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version) {
  _prevBtnState = HIGH;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _started_at = millis();
  _node_prefs = node_prefs;
  _display->turnOn();

  // strip off dash and commit hash by changing dash to null terminator
  // e.g: v1.2.3-abcdef -> v1.2.3
  char *version = strdup(firmware_version);
  char *dash = strchr(version, '-');
  if(dash){
    *dash = 0;
  }

  // v1.2.3 (1 Jan 2025)
  snprintf(_version_info, sizeof(_version_info), "%s (%s)", version, build_date);
  free(version);
}

void UITask::drawHeader(const char* title) {
  char buf[8];
  _display->setTextSize(1);
  _display->setColor(UIColor::primary_txt);

  sprintf(buf, "%d%%", battPercent(board.getBattMilliVolts()));
  _display->setCursor(0, 0);
  _display->print(buf);

  _display->drawTextCentered(_display->width() / 2, 0, title);

  // UTC. Shows whatever the RTC holds; on ESP32 that is a fixed boot-time seed until
  // GPS or an admin `time` command sets it - same as the companion's clock today.
  uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
  sprintf(buf, "%02lu:%02lu", (unsigned long)((now % 86400UL) / 3600), (unsigned long)((now % 3600UL) / 60));
  _display->drawTextRightAlign(_display->width() - 1, 0, buf);
}

void UITask::drawBreadcrumb() {
  int y = 10;
  int x = _display->width() / 2 - 5 * (PAGE_COUNT - 1);
  for (uint8_t i = 0; i < PAGE_COUNT; i++, x += 10) {
    if (i == _page) _display->fillRect(x - 1, y - 1, 4, 4);
    else            _display->fillRect(x, y, 2, 2);
  }
}

void UITask::renderCurrScreen() {
  char tmp[80];
  if (millis() < BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    uint16_t websiteWidth = _display->getTextWidth(website);
    _display->setCursor((_display->width() - websiteWidth) / 2, 22);
    _display->print(website);

    // version info
    _display->setTextSize(1);
    uint16_t versionWidth = _display->getTextWidth(_version_info);
    _display->setCursor((_display->width() - versionWidth) / 2, 35);
    _display->print(_version_info);

    // node type
    const char* node_type = "< Room Server >";
    uint16_t typeWidth = _display->getTextWidth(node_type);
    _display->setCursor((_display->width() - typeWidth) / 2, 48);
    _display->print(node_type);
  } else {
    switch (_page) {
      case PAGE_IDENTITY:
        drawHeader("IDENTITY");
        drawBreadcrumb();
        _display->drawTextEllipsized(0, 16, _display->width(), _node_prefs->node_name);
        _display->setCursor(0, 28);
        _display->print("< Room Server >");
        _display->setCursor(0, 40);
        sprintf(tmp, "F:%07.3f SF%d", _node_prefs->freq, _node_prefs->sf);
        _display->print(tmp);
        _display->setCursor(0, 52);
        sprintf(tmp, "BW:%.2f CR:%d", _node_prefs->bw, _node_prefs->cr);
        _display->print(tmp);
        break;
      case PAGE_MEMBERS:
        drawHeader("MEMBERS");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        sprintf(tmp, "Registered: %d", the_mesh.getNumRegisteredClients());
        _display->print(tmp);
        _display->setCursor(0, 28);
        sprintf(tmp, "Active today: %d", the_mesh.getNumActiveClients());
        _display->print(tmp);
        _display->setCursor(0, 40);
        sprintf(tmp, "Guest pwd: %s", _node_prefs->guest_password[0] ? "set" : "none");
        _display->print(tmp);
        break;
      case PAGE_STORAGE: {
        drawHeader("STORAGE");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        sprintf(tmp, "Posts: %d/%d", the_mesh.getPostQueueOccupancy(), MAX_UNSYNCED_POSTS);
        _display->print(tmp);
        _display->setCursor(0, 28);
        uint32_t age = the_mesh.getOldestQueuedPostAge();
        if (age == 0) strcpy(tmp, "Oldest: none");
        else formatAgo(tmp, "Oldest: ", age);
        _display->print(tmp);
#ifdef ESP32
        _display->setCursor(0, 40);
        sprintf(tmp, "Flash: %u/%uKB", (unsigned)(the_mesh.getCachedSpiffsUsedBytes() / 1024),
                (unsigned)(SPIFFS.totalBytes() / 1024));
        _display->print(tmp);
#endif
        break;
      }
      case PAGE_TRAFFIC: {
        drawHeader("TRAFFIC");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        sprintf(tmp, "S f:%lu d:%lu", (unsigned long)the_mesh.getNumSentFlood(), (unsigned long)the_mesh.getNumSentDirect());
        _display->print(tmp);
        _display->setCursor(0, 28);
        sprintf(tmp, "R f:%lu d:%lu", (unsigned long)the_mesh.getNumRecvFlood(), (unsigned long)the_mesh.getNumRecvDirect());
        _display->print(tmp);
        _display->setCursor(0, 40);
        sprintf(tmp, "RSSI:%d SNR:%.1f", (int)radio_driver.getLastRSSI(), radio_driver.getLastSNR());
        _display->print(tmp);
        _display->setCursor(0, 52);
        unsigned long air = the_mesh.getTotalAirTime() / 1000;
        sprintf(tmp, "Air %02lu:%02lu:%02lu", air / 3600, (air % 3600) / 60, air % 60);
        _display->print(tmp);
        break;
      }
      case PAGE_SYSTEM: {
        drawHeader("SYSTEM");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        unsigned long up = (millis() - _started_at) / 1000;
        sprintf(tmp, "Up %lud %02lu:%02lu", up / 86400, (up % 86400) / 3600, (up % 3600) / 60);
        _display->print(tmp);
#ifdef ESP32
        _display->setCursor(0, 28);
        sprintf(tmp, "Heap: %lu KB", (unsigned long)(ESP.getFreeHeap() / 1024));
        _display->print(tmp);
#endif
        _display->setCursor(0, 40);
        _display->print(_version_info);
        _display->setCursor(0, 52);
        uint32_t la = the_mesh.getLastAdminActivity();  // since boot (not persisted)
        if (la == 0) strcpy(tmp, "Admin: never");
        else formatAgo(tmp, "Admin: ", the_mesh.getRTCClock()->getCurrentTime() - la);
        _display->print(tmp);
        break;
      }
#if ENV_INCLUDE_GPS == 1
      case PAGE_GPS: {
        drawHeader("GPS");
        drawBreadcrumb();
        LocationProvider* nmea = sensors.getLocationProvider();
        _display->setCursor(0, 16);
        if (nmea == NULL) {
          _display->print("Can't access GPS");
        } else if (!_node_prefs->gps_enabled) {
          _display->print("GPS disabled");
        } else {
          sprintf(tmp, "fix:%s sat:%ld", nmea->isValid() ? "y" : "n", nmea->satellitesCount());
          _display->print(tmp);
          _display->setCursor(0, 28);
          sprintf(tmp, "%.4f %.4f", nmea->getLatitude() / 1000000., nmea->getLongitude() / 1000000.);
          _display->print(tmp);
        }
        break;
      }
#endif
      default:
        break;
    }
  }
}

void UITask::loop() {
#ifdef PIN_USER_BTN
  if (millis() >= _next_read) {
    int btnState = digitalRead(PIN_USER_BTN);
    if (btnState != _prevBtnState) {
      if (btnState == USER_BTN_PRESSED) {  // pressed?
        if (_display->isOn()) {
          _page = (_page + 1) % PAGE_COUNT;
        } else {
          _display->turnOn();
        }
        _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
      }
      _prevBtnState = btnState;
    }
    _next_read = millis() + 200;  // 5 reads per second
  }
#endif

  if (_display->isOn()) {
    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + 1000;   // refresh every second
    }
    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }
}
