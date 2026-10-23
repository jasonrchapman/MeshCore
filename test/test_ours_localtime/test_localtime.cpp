#include <gtest/gtest.h>

#define LOCALTIME_HOST_TEST 1
#include "helpers/LocalTime.cpp"

static const uint32_t JUL_1_2026 = 1782864000UL;  // 2026-07-01 00:00:00 UTC
static const uint32_t JAN_15_2026 = 1768435200UL; // 2026-01-15 00:00:00 UTC
static const long CHICAGO_LAT = 41878100L, CHICAGO_LON = -87629800L;
static const long DELHI_LAT = 28613900L, DELHI_LON = 77209000L;

TEST(LocalTime, NoFixIsUtc) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0);
  EXPECT_STREQ(lt.zoneName(), "UTC");
  EXPECT_EQ(lt.toLocal(JUL_1_2026), JUL_1_2026);
}

TEST(LocalTime, ChicagoSummerIsCdt) {
  LocalTime lt;
  lt.update(JUL_1_2026, true, CHICAGO_LAT, CHICAGO_LON);
  EXPECT_STREQ(lt.zoneName(), "CDT");
  EXPECT_EQ((int32_t)(lt.toLocal(JUL_1_2026) - JUL_1_2026), -5 * 3600);
}

TEST(LocalTime, ChicagoWinterIsCst) {
  LocalTime lt;
  lt.update(JAN_15_2026, true, CHICAGO_LAT, CHICAGO_LON);
  EXPECT_STREQ(lt.zoneName(), "CST");
  EXPECT_EQ((int32_t)(lt.toLocal(JAN_15_2026) - JAN_15_2026), -6 * 3600);
}

TEST(LocalTime, HalfHourZone) {
  LocalTime lt;
  lt.update(JUL_1_2026, true, DELHI_LAT, DELHI_LON);
  EXPECT_EQ((int32_t)(lt.toLocal(JUL_1_2026) - JUL_1_2026), 5 * 3600 + 30 * 60);
}

TEST(LocalTime, LostFixKeepsZone) {
  LocalTime lt;
  lt.update(JUL_1_2026, true, CHICAGO_LAT, CHICAGO_LON);
  lt.update(JUL_1_2026 + 3600, false, 0, 0);
  EXPECT_STREQ(lt.zoneName(), "CDT");
  EXPECT_EQ((int32_t)(lt.toLocal(JUL_1_2026 + 3600) - (JUL_1_2026 + 3600)), -5 * 3600);
}

TEST(LocalTime, TzStaysUnset) {
  unsetenv("TZ");
  LocalTime lt;
  lt.update(JUL_1_2026, true, CHICAGO_LAT, CHICAGO_LON);
  EXPECT_EQ(getenv("TZ"), nullptr);
}

TEST(LocalTimeFallback, ZeroIsUtc) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0, 0);
  EXPECT_STREQ(lt.zoneName(), "UTC");
  EXPECT_EQ(lt.toLocal(JUL_1_2026), JUL_1_2026);
}

TEST(LocalTimeFallback, NegativeOffset) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0, -6);
  EXPECT_STREQ(lt.zoneName(), "-0600");
  EXPECT_EQ((int32_t)(lt.toLocal(JUL_1_2026) - JUL_1_2026), -6 * 3600);
}

TEST(LocalTimeFallback, PositiveAndEdgeOffsets) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0, 5);
  EXPECT_STREQ(lt.zoneName(), "+0500");
  lt.update(JUL_1_2026, false, 0, 0, 14);
  EXPECT_STREQ(lt.zoneName(), "+1400");
  lt.update(JUL_1_2026, false, 0, 0, -12);
  EXPECT_STREQ(lt.zoneName(), "-1200");
}

TEST(LocalTimeFallback, ChangeAppliesAtOnce) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0, 2);
  lt.update(JUL_1_2026 + 1, false, 0, 0, 3);
  EXPECT_STREQ(lt.zoneName(), "+0300");
}

TEST(LocalTimeFallback, FixWinsOverOffset) {
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0, 3);
  lt.update(JUL_1_2026 + 60, true, CHICAGO_LAT, CHICAGO_LON, 3);
  EXPECT_STREQ(lt.zoneName(), "CDT");
}

TEST(LocalTimeFallback, LostFixKeepsGpsZoneNotOffset) {
  LocalTime lt;
  lt.update(JUL_1_2026, true, CHICAGO_LAT, CHICAGO_LON, 3);
  lt.update(JUL_1_2026 + 3600, false, 0, 0, 3);
  EXPECT_STREQ(lt.zoneName(), "CDT");
}

TEST(LocalTimeFallback, FourArgumentCallStaysUtc) {   // repeater and room server
  LocalTime lt;
  lt.update(JUL_1_2026, false, 0, 0);
  EXPECT_STREQ(lt.zoneName(), "UTC");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
