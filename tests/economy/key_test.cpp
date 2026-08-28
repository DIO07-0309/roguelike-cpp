#include <gtest/gtest.h>
#include "entities/player.h"

TEST(KeyTest, InitialZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.get_key_count(), 0);
}

TEST(KeyTest, AddKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(3);
    EXPECT_EQ(p.get_key_count(), 3);
}

TEST(KeyTest, SpendKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(2);
    EXPECT_TRUE(p.spend_key(1));
    EXPECT_EQ(p.get_key_count(), 1);
}

TEST(KeyTest, SpendInsufficient) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(1);
    EXPECT_FALSE(p.spend_key(2));
    EXPECT_EQ(p.get_key_count(), 1);
}
