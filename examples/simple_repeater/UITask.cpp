#include "UITask.h"
#include "target.h"
#include <Arduino.h>
#include <helpers/CommonCLI.h>
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

#if ENV_INCLUDE_GPS == 1
// Same lookup as the companion's UITask::getGPSState(): the live "gps" sensor setting.
static bool gpsActive() {
  int num = sensors.getNumSettings();
  for (int i = 0; i < num; i++) {
    if (strcmp(sensors.getSettingName(i), "gps") == 0) {
      return !strcmp(sensors.getSettingValue(i), "1");
    }
  }
  return false;
}
#endif

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      20000  // 20 seconds
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds

#define POWEROFF_DELAY 3000

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

#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  user_btn.begin();
#endif

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

  if (millis() >= _next_batt_read) {  // the ADC read is slow on some boards; 10s is plenty
    _batt_mv = _board->getBattMilliVolts();
    _next_batt_read = millis() + 10000;
  }
  sprintf(buf, "%d%%", battPercent(_batt_mv));
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
  if (millis() < _started_at + BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 22, website);

    // version info
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width() / 2, 35, _version_info);

    // node type
    const char* node_type = "< Repeater >";
    _display->drawTextCentered(_display->width() / 2, 48, node_type);
  } else if (_powering_off_at > 0) {
    // meshcore logo
    _display->setColor(UIColor::corp_blue);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    _display->setColor(UIColor::primary_txt);
    _display->setTextSize(1);
    _display->drawTextCentered(_display->width()/ 2, 22, website);

    // Powering off
    const char* poweroff_string = "Turning OFF";
    uint16_t poffWidth = _display->getTextWidth(poweroff_string);
    _display->setCursor((_display->width() - poffWidth) / 2, 48);
    _display->drawTextCentered(_display->width()/2, 48, poweroff_string);
  } else {
    switch (_page) {
      case PAGE_IDENTITY:
        drawHeader("IDENTITY");
        drawBreadcrumb();
        _display->drawTextEllipsized(0, 16, _display->width(), _node_prefs->node_name);
        _display->setCursor(0, 28);
        _display->print("< Repeater >");
        _display->setCursor(0, 40);
        sprintf(tmp, "F:%07.3f SF%d", _node_prefs->freq, _node_prefs->sf);
        _display->print(tmp);
        _display->setCursor(0, 52);
        sprintf(tmp, "BW:%.2f CR:%d", _node_prefs->bw, _node_prefs->cr);
        _display->print(tmp);
        break;
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
        unsigned long air = the_mesh.getTotalAirTime() / 1000;
        sprintf(tmp, "Air %02lu:%02lu:%02lu", air / 3600, (air % 3600) / 60, air % 60);
        _display->print(tmp);
        _display->setCursor(0, 52);
        sprintf(tmp, "TX budget: %u%%", the_mesh.getTxBudgetPercent());
        _display->print(tmp);
        break;
      }
      case PAGE_SIGNAL: {
        drawHeader("SIGNAL");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        sprintf(tmp, "RSSI: %d dBm", (int)radio_driver.getLastRSSI());
        _display->print(tmp);
        _display->setCursor(0, 28);
        sprintf(tmp, "SNR: %.1f dB", radio_driver.getLastSNR());
        _display->print(tmp);
        _display->setCursor(0, 40);
        sprintf(tmp, "Noise: %d dBm", radio_driver.getNoiseFloor());
        _display->print(tmp);
        _display->setCursor(0, 52);
        uint32_t lh = the_mesh.getLastHeardMillis();
        if (lh == 0) strcpy(tmp, "Heard: never");
        else formatAgo(tmp, "Heard: ", (millis() - lh) / 1000);
        _display->print(tmp);
        break;
      }
      case PAGE_SYSTEM: {
        drawHeader("SYSTEM");
        drawBreadcrumb();
        _display->setCursor(0, 16);
        unsigned long up = the_mesh.getUptimeSecs();  // 64-bit based, no 49-day wrap
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
        uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
        if (la == 0) strcpy(tmp, "Admin: never");
        else if (now < la || now - la > the_mesh.getUptimeSecs()) strcpy(tmp, "Admin: this boot");  // RTC jumped
        else formatAgo(tmp, "Admin: ", now - la);
        _display->print(tmp);
        break;
      }
#if ENV_INCLUDE_GPS == 1
      case PAGE_GPS: {
        drawHeader("GPS");
        drawBreadcrumb();
        // Mirrors the companion's GPS page (ui-new/UITask.cpp HomePage::GPS), display-only.
        LocationProvider* nmea = sensors.getLocationProvider();
        int y = 18;
        bool gps_state = gpsActive();
#ifdef PIN_GPS_SWITCH
        bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
        if (gps_state != hw_gps_state) {
          strcpy(tmp, gps_state ? "gps off(hw)" : "gps off(sw)");
        } else {
          strcpy(tmp, gps_state ? "gps on" : "gps off");
        }
#else
        strcpy(tmp, gps_state ? "gps on" : "gps off");
#endif
        _display->setColor(UIColor::primary_txt);
        _display->drawTextLeftAlign(0, y, tmp);
        if (nmea == NULL) {
          y = y + 12;
          _display->setColor(UIColor::secondary_txt);
          _display->drawTextLeftAlign(0, y, "Can't access GPS");
        } else {
          _display->setColor(UIColor::primary_txt);
          strcpy(tmp, nmea->isValid() ? "fix" : "no fix");
          _display->drawTextRightAlign(_display->width() - 1, y, tmp);
          y = y + 12;
          _display->setColor(UIColor::secondary_txt);
          _display->drawTextLeftAlign(0, y, "sat");
          float hdop = nmea->getHDOP();
          if (nmea->isValid() && hdop > 0) {
            // 10 segments, more filled = better accuracy (lower HDOP)
            int filled = hdop <= 1.0f ? 10 : (int)(10.0f / hdop);
            _display->setColor(UIColor::primary_txt);
            for (int i = 0; i < 10; i++) {
              if (i < filled) _display->fillRect(24 + i * 4, y, 3, 6);
              else            _display->drawRect(24 + i * 4, y, 3, 6);
            }
          }
          _display->setColor(UIColor::primary_txt);
          sprintf(tmp, "%ld", nmea->satellitesCount());
          _display->drawTextRightAlign(_display->width() - 1, y, tmp);
          y = y + 12;
          _display->setColor(UIColor::secondary_txt);
          _display->drawTextLeftAlign(0, y, "pos");
          _display->setColor(UIColor::primary_txt);
          sprintf(tmp, "%.4f %.4f", nmea->getLatitude() / 1000000., nmea->getLongitude() / 1000000.);
          _display->drawTextRightAlign(_display->width() - 1, y, tmp);
          y = y + 12;
          _display->setColor(UIColor::secondary_txt);
          _display->drawTextLeftAlign(0, y, "alt");
          _display->setColor(UIColor::primary_txt);
          sprintf(tmp, "%.2f", nmea->getAltitude() / 1000.);
          _display->drawTextRightAlign(_display->width() - 1, y, tmp);
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
#if defined(PIN_USER_BTN) && defined(DISPLAY_CLASS)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    if (_display->isOn()) {
      _page = (_page + 1) % PAGE_COUNT;
    } else {
      _display->turnOn();
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      _display->turnOn();
      Serial.println("Powering Off");
      _powering_off_at = millis() + POWEROFF_DELAY; 
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

  if (_powering_off_at > 0) { // power off timer armed
#if defined(LED_PIN) && defined(LED_STATE_ON)
    digitalWrite(LED_PIN, LED_STATE_ON); // switch on the led until poweroff
    delay(1000);
    digitalWrite(LED_PIN, !LED_STATE_ON); // off LED
#endif
    if (millis() > _powering_off_at) {
      _board->powerOff();  // should not return
    }
  }
}
