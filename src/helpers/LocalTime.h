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

// --- Clock-is-set check (our addition) ---
// A time earlier than the day before this build cannot be real. It is a start time (upstream's
// ESP32 clock starts at RTC_TIME_MIN, 1 Mar 2026), a time saved before a reboot that was never set,
// a stale contact time, or clkreboot's 2024 time. The screen clocks show --:-- for it.
// __DATE__ ("Mmm dd yyyy") is the build PC's local date, so one day of margin covers every zone.
// C++11 constexpr: one return expression per function.
constexpr int localtimeMonth(const char* d) {
  return d[0] == 'J' ? (d[1] == 'a' ? 1 : (d[2] == 'n' ? 6 : 7)) :
         d[0] == 'F' ? 2 :
         d[0] == 'M' ? (d[2] == 'r' ? 3 : 5) :
         d[0] == 'A' ? (d[1] == 'p' ? 4 : 8) :
         d[0] == 'S' ? 9 : d[0] == 'O' ? 10 : d[0] == 'N' ? 11 : 12;
}
constexpr int localtimeDay(const char* d) { return (d[4] == ' ' ? 0 : d[4] - '0') * 10 + (d[5] - '0'); }
constexpr int localtimeYear(const char* d) {
  return (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0');
}
// Days since 1970-01-01 (H. Hinnant's days_from_civil), with y already moved back for Jan and Feb.
constexpr long localtimeDaysY(long y, int m, int d) {
  return 365L * y + y / 4 - y / 100 + y / 400 + (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1 - 719468L;
}
constexpr uint32_t localtimeDateToEpoch(const char* d) {
  return (uint32_t)(localtimeDaysY(localtimeYear(d) - (localtimeMonth(d) <= 2 ? 1 : 0),
                                   localtimeMonth(d), localtimeDay(d)) * 86400L);
}

#ifndef LOCALTIME_BUILD_EPOCH
  #define LOCALTIME_BUILD_EPOCH (localtimeDateToEpoch(__DATE__) - 86400UL)
#endif

inline bool clockIsRealAt(uint32_t now, uint32_t build_epoch) { return now >= build_epoch; }

// True once the RTC holds a time from after this build: set by the app, the GPS or a command.
// Display only. Uses LOCALTIME_BUILD_EPOCH as compiled into LocalTime.cpp.
bool clockIsReal(uint32_t now);
