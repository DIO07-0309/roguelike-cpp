#include <gtest/gtest.h>
#include "entities/player.h"
#include "world/special_room.h"
#include "reward_manager.h"
#include "combat_system.h"
#include "item.h"

TEST(GambleCostTest, CostScalesWithFloor) {
    EXPECT_EQ(get_gamble_cost(1), 50);
    EXPECT_EQ(get_gamble_cost(5), 90);
    EXPECT_EQ(get_gamble_cost(10), 140);
    EXPECT_EQ(get_gamble_cost(15), 190);
}

TEST(GambleCostTest, CostFormula) {
    for (int f = 1; f <= 15; f++)
        EXPECT_EQ(get_gamble_cost(f), 40 + f * 10);
}

TEST(GambleRoomTest, PlayerFloorContext) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.current_floor, 1);
    p.current_floor = 5;
    EXPECT_EQ(p.gold, 0);
    EXPECT_EQ(p.get_gold(), 0);
}

TEST(GambleRoomTest, GoldSpendMatchesCost) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(200);
    p.current_floor = 3;
    int cost = get_gamble_cost(p.current_floor);
    EXPECT_EQ(cost, 70);
    EXPECT_TRUE(p.spend_gold(cost));
    EXPECT_EQ(p.get_gold(), 130);
}

TEST(GambleRoomTest, InsufficientGoldBlocks) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(30);
    p.current_floor = 1;
    int cost = get_gamble_cost(p.current_floor);
    EXPECT_EQ(cost, 50);
    EXPECT_FALSE(p.spend_gold(cost));
    EXPECT_EQ(p.get_gold(), 30);
}

TEST(GambleRoomTest, RepeatedSpendPossible) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(500);
    p.current_floor = 1;
    int cost = get_gamble_cost(1);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(p.spend_gold(cost));
    EXPECT_EQ(p.get_gold(), 500 - 5 * cost);
}

TEST(GambleRoomTest, GrantKeyViaRewardManager) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    int before = p.key_count;
    RewardManager::grant_key(p, 1);
    EXPECT_EQ(p.key_count, before + 1);
}

TEST(GambleRoomTest, RelicPersistenceRunScope) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    bool ok = RewardManager::grant_relic(p, "blood_charm", PersistenceScope::RUN);
    if (ok) {
        bool found = false;
        for (auto& r : p.relics)
            if (r.id == "blood_charm" && r.scope == PersistenceScope::RUN)
                found = true;
        EXPECT_TRUE(found);
        p.relics.erase(
            std::remove_if(p.relics.begin(), p.relics.end(),
                [](const RelicInstance& r) { return r.scope == PersistenceScope::FLOOR; }),
            p.relics.end());
        found = false;
        for (auto& r : p.relics)
            if (r.id == "blood_charm") found = true;
        EXPECT_TRUE(found);
    }
}

TEST(GambleRoomTest, RelicExhaustionNoCrash) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto all_ids = get_all_relic_ids();
    for (auto& id : all_ids)
        p.relics.push_back({id, PersistenceScope::RUN});
    for (auto& id : all_ids) {
        EXPECT_TRUE(player_has_relic(&p, id));
    }
    RewardManager::grant_key(p, 1);
    EXPECT_EQ(p.key_count, 1);
}
