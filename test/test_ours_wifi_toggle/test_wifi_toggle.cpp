#include <gtest/gtest.h>
#include <string>
#include "../../examples/companion_radio/WifiToggle.h"

// Records each step, so a test can compare the order and the count.
struct FakeOps : WifiToggle::Ops {
  std::string log;
  bool ssid = true;
  void savePref(bool on) override { log += on ? "save1 " : "save0 "; }
  bool loadSsid() override { log += "load "; return ssid; }
  void serverBegin() override { log += "server "; }
  void attach() override { log += "attach "; }
  void detach() override { log += "detach "; }
  void linkStart() override { log += "start "; }
  void linkStop() override { log += "stop "; }
};

TEST(WifiToggle, OffAtBootThenOnStartsServerOnce) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(false);
  EXPECT_TRUE(t.set(true, false, true, ops));
  EXPECT_EQ(ops.log, "save1 load server attach start ");
}

TEST(WifiToggle, OnAtBootThenOffThenOnReusesServer) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(true);
  EXPECT_FALSE(t.set(false, true, true, ops));
  EXPECT_TRUE(t.set(true, false, true, ops));
  EXPECT_EQ(ops.log, "save0 stop detach save1 load attach start ");
}

TEST(WifiToggle, RepeatedCyclesStartServerOnlyOnce) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(false);
  t.set(true, false, true, ops);
  t.set(false, true, true, ops);
  t.set(true, false, true, ops);
  EXPECT_EQ(ops.log, "save1 load server attach start save0 stop detach save1 load attach start ");
}

TEST(WifiToggle, NoSavedNetworkStaysOff) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(false);
  ops.ssid = false;
  EXPECT_FALSE(t.set(true, false, true, ops));
  EXPECT_EQ(ops.log, "save1 load ");
}

TEST(WifiToggle, NetworkClearedByTheAppTurnsWifiOff) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(true);
  ops.ssid = false;
  EXPECT_FALSE(t.set(true, true, true, ops));
  EXPECT_EQ(ops.log, "save1 load stop detach ");
}

TEST(WifiToggle, OnWhileOnRestartsWithTheSavedNetwork) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(true);
  EXPECT_TRUE(t.set(true, true, true, ops));
  EXPECT_EQ(ops.log, "save1 load stop start ");
}

TEST(WifiToggle, NotLiveOnlySaves) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(false);
  EXPECT_FALSE(t.set(true, false, false, ops));
  EXPECT_EQ(ops.log, "save1 ");
}

TEST(WifiToggle, OnlyTheFirstNoteBootCounts) {
  WifiToggle t; FakeOps ops;
  t.noteBoot(true);
  t.noteBoot(false);
  t.set(false, true, true, ops);
  t.set(true, false, true, ops);
  EXPECT_EQ(ops.log, "save0 stop detach save1 load attach start ");
}

TEST(WifiToggle, SignalWords) {
  EXPECT_STREQ(wifiSignalWord(-40), "good");
  EXPECT_STREQ(wifiSignalWord(-67), "good");
  EXPECT_STREQ(wifiSignalWord(-68), "fair");
  EXPECT_STREQ(wifiSignalWord(-75), "fair");
  EXPECT_STREQ(wifiSignalWord(-76), "weak");
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
