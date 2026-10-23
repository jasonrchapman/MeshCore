#include <gtest/gtest.h>

#define LOCALTIME_HOST_TEST 1
#include "helpers/LocalTime.cpp"

static const uint32_t OCT_21_2026 = 1792540800UL;   // build epoch of a build on 22 Oct 2026

TEST(ClockReal, ParsesBuildDates) {
  EXPECT_EQ(localtimeDateToEpoch("Oct 22 2026"), 1792627200UL);
  EXPECT_EQ(localtimeDateToEpoch("Jan  5 2027"), 1799107200UL);   // __DATE__ pads the day with a space
  EXPECT_EQ(localtimeDateToEpoch("Mar  1 2026"), 1772323200UL);   // upstream's RTC_TIME_MIN
  EXPECT_EQ(localtimeDateToEpoch("Feb 29 2024"), 1709164800UL);
}

TEST(ClockReal, ParsesEveryMonth) {
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int m = 0; m < 12; m++) {
    char s[12];
    snprintf(s, sizeof s, "%s 15 2026", months[m]);
    EXPECT_EQ(localtimeMonth(s), m + 1) << s;
  }
}

TEST(ClockReal, BuildEpochIsTheDayBeforeTheBuildDate) {
  EXPECT_EQ(LOCALTIME_BUILD_EPOCH, localtimeDateToEpoch(__DATE__) - 86400UL);
  EXPECT_GT(LOCALTIME_BUILD_EPOCH, 1772323200UL);   // later than upstream's 1 Mar 2026 start time
}

TEST(ClockReal, StartTimeIsNotReal) {
  EXPECT_FALSE(clockIsRealAt(1772323200UL + 600, OCT_21_2026));   // 10 min after a power-on
  EXPECT_FALSE(clockIsRealAt(1772323200UL + 7 * 86400, OCT_21_2026));   // a week of uptime, never set
}

TEST(ClockReal, ClkrebootTimeIsNotReal) {
  EXPECT_FALSE(clockIsRealAt(1715770351UL, OCT_21_2026));   // CommonCLI clkreboot: 15 May 2024
}

TEST(ClockReal, TimeFromTheBuildDayOnIsReal) {
  EXPECT_FALSE(clockIsRealAt(OCT_21_2026 - 1, OCT_21_2026));
  EXPECT_TRUE(clockIsRealAt(OCT_21_2026, OCT_21_2026));
  EXPECT_TRUE(clockIsRealAt(1800000000UL, OCT_21_2026));
}

TEST(ClockReal, ClockIsRealUsesTheBuildEpoch) {
  EXPECT_FALSE(clockIsReal(LOCALTIME_BUILD_EPOCH - 1));
  EXPECT_TRUE(clockIsReal(LOCALTIME_BUILD_EPOCH));
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
