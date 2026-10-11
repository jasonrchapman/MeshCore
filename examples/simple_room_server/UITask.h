#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>

class UITask {
  DisplayDriver* _display;
  unsigned long _next_read, _next_refresh, _auto_off;
  int _prevBtnState;
  NodePrefs* _node_prefs;
  char _version_info[32];
  enum Page {
    PAGE_IDENTITY, PAGE_MEMBERS, PAGE_STORAGE, PAGE_TRAFFIC, PAGE_SYSTEM,
#if ENV_INCLUDE_GPS == 1
    PAGE_GPS,
#endif
    PAGE_COUNT
  };
  uint8_t _page = PAGE_IDENTITY;

  void drawHeader(const char* title);
  void drawBreadcrumb();

  void renderCurrScreen();
public:
  UITask(DisplayDriver& display) : _display(&display) { _next_read = _next_refresh = 0; }
  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version);

  void loop();
};