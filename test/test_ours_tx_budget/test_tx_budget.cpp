#include <gtest/gtest.h>
#include "helpers/ui/TxBudgetFormat.h"

TEST(TxBudget, ZeroMaxIsZero) { EXPECT_EQ(txBudgetPercent(50, 0), 0); }
TEST(TxBudget, Half) { EXPECT_EQ(txBudgetPercent(50, 100), 50); }
TEST(TxBudget, FullAndOverIsHundred) {
  EXPECT_EQ(txBudgetPercent(100, 100), 100);
  EXPECT_EQ(txBudgetPercent(150, 100), 100);
}
TEST(TxBudget, RoundsDown) { EXPECT_EQ(txBudgetPercent(1, 3), 33); }
TEST(TxBudget, NoOverflowAtLargeBudget) { EXPECT_EQ(txBudgetPercent(3600000UL, 7200000UL), 50); }

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
