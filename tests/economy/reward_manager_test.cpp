#include <gtest/gtest.h>
#include "entities/player.h"
#include "systems/reward_manager.h"

TEST(RewardManagerTest, GrantGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    RewardManager::grant_gold(p, 100);
    EXPECT_EQ(p.get_gold(), 100);
}

TEST(RewardManagerTest, GrantKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    RewardManager::grant_key(p, 2);
    EXPECT_EQ(p.get_key_count(), 2);
}

TEST(RewardManagerTest, GrantItemNull) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_FALSE(RewardManager::grant_item(p, nullptr));
}
