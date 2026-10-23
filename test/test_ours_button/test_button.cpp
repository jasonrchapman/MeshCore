#include <gtest/gtest.h>

// Arduino pin calls that upstream's mock (test/mocks/Arduino.h) does not have
#define LOW 0
#define HIGH 1
#define INPUT 0
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
static int g_level = HIGH;
inline int digitalRead(int) { return g_level; }
inline int analogRead(int) { return 0; }
inline void pinMode(int, int) {}

#include "helpers/ui/MomentaryButton.cpp"

struct Events { int clicks = 0, doubles = 0, longs = 0; };

static void run(MomentaryButton& b, Events& ev, int level, int ms) {
  g_level = level;
  for (int i = 0; i < ms; i++) {
    g_mock_millis++;
    int e = b.check();
    if (e == BUTTON_EVENT_CLICK) ev.clicks++;
    if (e == BUTTON_EVENT_DOUBLE_CLICK) ev.doubles++;
    if (e == BUTTON_EVENT_LONG_PRESS) ev.longs++;
  }
}

TEST(Button, ChatterIsOneClick) {
  g_mock_millis = 1000; g_level = HIGH;
  MomentaryButton b(0, 1000, true, true);   // active low, long press 1 s
  b.begin();
  Events ev;
  run(b, ev, LOW, 3); run(b, ev, HIGH, 2); run(b, ev, LOW, 3); run(b, ev, HIGH, 2);   // press chatter
  run(b, ev, LOW, 150);                                                               // held
  run(b, ev, HIGH, 3); run(b, ev, LOW, 2); run(b, ev, HIGH, 600);                     // release chatter
  EXPECT_EQ(ev.clicks, 1);
  EXPECT_EQ(ev.doubles, 0);
}

TEST(Button, TwoRealPressesAreADoubleClick) {
  g_mock_millis = 1000; g_level = HIGH;
  MomentaryButton b(0, 1000, true, true);
  b.begin();
  Events ev;
  run(b, ev, LOW, 100); run(b, ev, HIGH, 100); run(b, ev, LOW, 100); run(b, ev, HIGH, 600);
  EXPECT_EQ(ev.doubles, 1);
  EXPECT_EQ(ev.clicks, 0);
}

TEST(Button, LongPress) {
  g_mock_millis = 1000; g_level = HIGH;
  MomentaryButton b(0, 1000, true, true);
  b.begin();
  Events ev;
  run(b, ev, LOW, 1200); run(b, ev, HIGH, 600);
  EXPECT_EQ(ev.longs, 1);
  EXPECT_EQ(ev.clicks, 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
