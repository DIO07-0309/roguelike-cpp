#include <gtest/gtest.h>
#include "entities/player.h"

TEST(GoldTest, InitialZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.get_gold(), 0);
}

TEST(GoldTest, AddGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(50);
    EXPECT_EQ(p.get_gold(), 50);
    p.add_gold(30);
    EXPECT_EQ(p.get_gold(), 80);
}

TEST(GoldTest, SpendGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(100);
    EXPECT_TRUE(p.spend_gold(40));
    EXPECT_EQ(p.get_gold(), 60);
}

TEST(GoldTest, SpendInsufficient) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(10);
    EXPECT_FALSE(p.spend_gold(50));
    EXPECT_EQ(p.get_gold(), 10);
}

TEST(GoldTest, AddNegativeNoop) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(-10);
    EXPECT_EQ(p.get_gold(), 0);
}
