#include "LocalTime.h"

#include <Arduino.h>  // chip-family macros (sdkconfig.h on ESP32)
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The grid (about 132 KB of flash) only goes into chip families measured to have room for it with
// the companion build, the largest of the three roles (2026-10-15, grid forced on):
//   nRF52840  RAK4631 86 KB free, T-Echo 177 KB, ThinkNode M1 182 KB  (slot 712,704)
//   ESP32     T-Beam 4 MB 361 KB free, Heltec V2 1.79 MB
//   ESP32-S3  Heltec V3 1.94 MB free, Heltec V4 4.4 MB
// Every other chip family builds without it, and its screen clocks show UTC. If a board in these
// families ever overflows flash, the link fails, so the image never ships broken.
// LOCALTIME_HOST_TEST turns the grid on for the host unit tests in test/test_ours_localtime.
#if defined(NRF52840_XXAA) || defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(LOCALTIME_HOST_TEST)
  #define LOCALTIME_GRID 1
  #include "TimeZoneGrid.h"
#endif

#ifdef LOCALTIME_GRID
// TZ_RULES index for a position: the row's runs, then the last run starting at or before the column.
static int lookupRule(long lat_e6, long lon_e6) {
  long long r = ((long long)lat_e6 + 90000000LL) * TZ_GRID_CELLS_PER_DEGREE / 1000000LL;
  long long c = ((long long)lon_e6 + 180000000LL) * TZ_GRID_CELLS_PER_DEGREE / 1000000LL;
  if (r < 0) r = 0;
  if (r >= TZ_GRID_ROWS) r = TZ_GRID_ROWS - 1;
  if (c < 0) c = 0;
  if (c >= TZ_GRID_COLS) c = TZ_GRID_COLS - 1;
  uint32_t lo = TZ_ROW_START[r], hi = TZ_ROW_START[r + 1] - 1;  // every row has a run at column 0
  while (lo < hi) {
    uint32_t mid = (lo + hi + 1) / 2;
    if (TZ_RUN_COL[mid] <= c) lo = mid;
    else hi = mid - 1;
  }
  return TZ_RUN_RULE[lo];
}

// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's days_from_civil).
static long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}
#endif

void LocalTime::update(uint32_t utc, bool have_fix, long lat_e6, long lon_e6) {
#ifdef LOCALTIME_GRID
  int rule = have_fix ? lookupRule(lat_e6, lon_e6) : _rule;
  if (rule == _rule && utc < _valid_until) return;
  _rule = rule;
  _valid_until = (utc / 900 + 1) * 900;  // next quarter hour

  if (rule == 0) {
    _offset = 0;
    strcpy(_abbr, "UTC");
    return;
  }

  // Apply the zone's rule just long enough to convert, then restore whatever TZ was before.
  const char* prev = getenv("TZ");
  char saved[48] = "";
  if (prev) strncpy(saved, prev, sizeof(saved) - 1);
  setenv("TZ", TZ_RULES[rule], 1);
  tzset();
  time_t t = (time_t)utc;
  struct tm tm;
  localtime_r(&t, &tm);
  strftime(_abbr, sizeof(_abbr), "%Z", &tm);
  if (prev) setenv("TZ", saved, 1);
  else unsetenv("TZ");
  tzset();

  int64_t local = (int64_t)daysFromCivil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday) * 86400
                + tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec;  // 64-bit: past 2038 on 32-bit long
  _offset = (int32_t)(local - (int64_t)utc);
#else
  (void)utc; (void)have_fix; (void)lat_e6; (void)lon_e6;
#endif
}

bool clockIsReal(uint32_t now) { return clockIsRealAt(now, LOCALTIME_BUILD_EPOCH); }
