#pragma once

#include <stdint.h>

// Local wall-clock time for on-screen clocks, from the GPS position. Display only: the RTC,
// every packet timestamp and the logs stay UTC.
//
// The zone comes from a lat/lon grid of POSIX TZ rules (TimeZoneGrid.h); the C library applies
// the rule, daylight saving included. TZ is set only while the offset is worked out and is put
// back straight away, because other code (the RX8130CE RTC driver's mktime()) assumes UTC. That
// happens when the position moves to another grid cell, and at each quarter hour - every zone
// changes offset on a quarter-hour boundary. Between those, toLocal() is one addition.
class LocalTime {
public:
  // Call before drawing. have_fix/lat/lon from the LocationProvider (millionths of a degree).
  // Without a fix the last zone found this boot is kept; before the first fix it is UTC.
  void update(uint32_t utc, bool have_fix, long lat_e6, long lon_e6);

  uint32_t toLocal(uint32_t utc) const { return utc + _offset; }
  const char* zoneName() const { return _abbr; }  // e.g. "CDT", "+0530", or "UTC"

private:
  int _rule = 0;              // TZ_RULES index, 0 = UTC
  uint32_t _valid_until = 0;  // recompute _offset/_abbr at or after this UTC time
  int32_t _offset = 0;        // local minus UTC, seconds
  char _abbr[8] = "UTC";
};
