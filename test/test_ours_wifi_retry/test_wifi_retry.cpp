#include <gtest/gtest.h>
#include "../../examples/companion_radio/WifiRetry.h"

typedef WifiRetry R;

// Calls poll() each second until it returns an action or "limit" ms pass. Moves "now" along.
static R::Action runUntilAction(R& r, uint32_t& now, uint32_t limit) {
  for (uint32_t end = now + limit; now != end; ) {
    now += 1000;
    R::Action a = r.poll(now);
    if (a != R::DO_NOTHING) return a;
  }
  return R::DO_NOTHING;
}

TEST(WifiRetry, StartTriesAtOnce) {
  R r;
  EXPECT_EQ(r.start(1000), R::START_TRY);
  EXPECT_EQ(r.state(), R::CONNECTING);
}

TEST(WifiRetry, FailedTryTurnsRadioOffAndWaitsTenSeconds) {
  R r; uint32_t now = 0;
  r.start(now);
  EXPECT_EQ(runUntilAction(r, now, 20000), R::STOP_RADIO);
  EXPECT_EQ(now, R::ATTEMPT_MS);
  EXPECT_EQ(r.state(), R::WAITING);
  EXPECT_EQ(r.reason(), R::NO_NETWORK);
  EXPECT_EQ(r.secondsToNextTry(now), 10u);
  EXPECT_EQ(runUntilAction(r, now, 20000), R::START_TRY);
  EXPECT_EQ(now, R::ATTEMPT_MS + 10000);
}

TEST(WifiRetry, WaitsGrowThenStayAtFiveMinutes) {
  EXPECT_EQ(R::waitAfter(1), 10000u);
  EXPECT_EQ(R::waitAfter(2), 30000u);
  EXPECT_EQ(R::waitAfter(3), 60000u);
  EXPECT_EQ(R::waitAfter(4), 120000u);
  EXPECT_EQ(R::waitAfter(5), 300000u);
  EXPECT_EQ(R::waitAfter(50), 300000u);
}

TEST(WifiRetry, GivesUpAfterFifteenMinutesWithoutALink) {
  R r; uint32_t now = 0;
  r.start(now);
  int tries = 1;
  while (r.state() != R::GAVE_UP && now < 30UL * 60 * 1000) {
    if (runUntilAction(r, now, 10UL * 60 * 1000) == R::START_TRY) tries++;
  }
  EXPECT_EQ(r.state(), R::GAVE_UP);
  EXPECT_GE(now, R::GIVE_UP_MS);
  EXPECT_LT(now, R::GIVE_UP_MS + 6UL * 60 * 1000);
  EXPECT_EQ(tries, 7);   // tries start at 0 s, 25 s, 70 s, 145 s, 280 s, 595 s, 910 s
  EXPECT_EQ(runUntilAction(r, now, 60UL * 60 * 1000), R::DO_NOTHING);   // stays off
}

TEST(WifiRetry, WrongPasswordGivesUpAfterThreeTries) {
  R r; uint32_t now = 0;
  r.start(now);
  for (int i = 0; i < 3; i++) {
    now += 2000;
    EXPECT_EQ(r.onDisconnected(now, true), R::STOP_RADIO);
    if (i < 2) EXPECT_EQ(runUntilAction(r, now, 10UL * 60 * 1000), R::START_TRY);
  }
  EXPECT_EQ(r.state(), R::GAVE_UP);
  EXPECT_EQ(r.reason(), R::WRONG_PASSWORD);
}

TEST(WifiRetry, ShortLinksStillReachTheGiveUpTime) {   // weak WiFi: joins, drops after 1 min
  R r; uint32_t now = 0;
  r.start(now);
  while (r.state() != R::GAVE_UP && now < 60UL * 60 * 1000) {
    now += 3000;
    r.onConnected(now);
    now += 60000;
    r.poll(now);
    r.onDisconnected(now, false);
    if (r.state() == R::GAVE_UP) break;
    runUntilAction(r, now, 10UL * 60 * 1000);
  }
  EXPECT_EQ(r.state(), R::GAVE_UP);
  EXPECT_EQ(now, 1261000u);   // the first failure after 15 min: the 7th drop, at 21 min 1 s
}

TEST(WifiRetry, DropFromStableLinkStartsFresh) {
  R r; uint32_t now = 0;
  r.start(now);
  now += 5000;
  r.onConnected(now);
  EXPECT_EQ(r.state(), R::CONNECTED);
  now += R::STABLE_MS + 1000;
  r.poll(now);
  now += 60UL * 60 * 1000;   // an hour later the access point goes away
  EXPECT_EQ(r.onDisconnected(now, false), R::STOP_RADIO);
  EXPECT_EQ(r.state(), R::WAITING);
  EXPECT_EQ(r.secondsToNextTry(now), 10u);
}

TEST(WifiRetry, StopTurnsOffAndIgnoresLateEvents) {
  R r; uint32_t now = 0;
  r.start(now);
  EXPECT_EQ(r.stop(), R::STOP_RADIO);
  EXPECT_EQ(r.state(), R::OFF);
  EXPECT_EQ(r.onDisconnected(now + 100, false), R::DO_NOTHING);
  r.onConnected(now + 200);
  EXPECT_EQ(r.state(), R::OFF);
  EXPECT_EQ(runUntilAction(r, now, 60UL * 60 * 1000), R::DO_NOTHING);
}

TEST(WifiRetry, StartAfterGiveUpBeginsAgain) {
  R r; uint32_t now = 0;
  r.start(now);
  for (int i = 0; i < 3; i++) { now += 1000; r.onDisconnected(now, true); runUntilAction(r, now, 600000); }
  ASSERT_EQ(r.state(), R::GAVE_UP);
  EXPECT_EQ(r.start(now), R::START_TRY);
  EXPECT_EQ(r.state(), R::CONNECTING);
  EXPECT_EQ(r.reason(), R::NO_REASON);
}

TEST(WifiRetry, LateConnectAfterTimeoutIsIgnored) {
  R r; uint32_t now = 0;
  r.start(now);
  EXPECT_EQ(runUntilAction(r, now, 20000), R::STOP_RADIO);   // the try timed out, the radio is off
  r.onConnected(now + 10);                                    // the event arrives just too late
  EXPECT_EQ(r.state(), R::WAITING);
}

TEST(WifiRetry, MillisWrapIsSafe) {
  R r; uint32_t now = 0xFFFFFFFFUL - 5000;
  r.start(now);
  EXPECT_EQ(runUntilAction(r, now, 20000), R::STOP_RADIO);
  EXPECT_EQ(runUntilAction(r, now, 20000), R::START_TRY);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
