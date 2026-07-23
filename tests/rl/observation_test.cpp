// G8.4: Observation tests
#include <gtest/gtest.h>
#include "ai/rl/observation.h"
#include "ai/mcts/simulation_state.h"

using namespace rl;
using namespace mcts;

TEST(Observation, FixedLengthVector) {
    SimulationState s;
    s.player.hp = 80; s.player.max_hp = 100;
    s.player.attack = 10;
    s.monsters.push_back(MonsterSnapshot{"orc", 30, 30, 5, 0, 7, 3, 1, {}, true});
    auto obs = Observation::from_state(s);
    auto vec = obs.to_vector();
    EXPECT_EQ(vec.size(), 7u); // 7 features
    EXPECT_NEAR(vec[0], 0.8f, 0.01f);  // hp_ratio
    EXPECT_FLOAT_EQ(vec[1], 10.0f);    // attack
    EXPECT_FLOAT_EQ(vec[2], 1.0f);     // enemy_count
}

TEST(Observation, KeyIsDeterministic) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    auto obs = Observation::from_state(s);
    auto k1 = obs.to_key();
    auto k2 = obs.to_key();
    EXPECT_EQ(k1, k2);
}

TEST(Observation, KeyChangesWithHP) {
    SimulationState s1, s2;
    s1.player.hp = 100; s1.player.max_hp = 100;
    s2.player.hp = 50;  s2.player.max_hp = 100;
    auto k1 = Observation::from_state(s1).to_key();
    auto k2 = Observation::from_state(s2).to_key();
    EXPECT_NE(k1, k2);
}

TEST(Observation, BossPresentFlag) {
    SimulationState s;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"boss", 200, 200, 3, 0, 25, 10, 8, {}, true, true});
    auto obs = Observation::from_state(s);
    EXPECT_FLOAT_EQ(obs.boss_present, 1.0f);
}

TEST(Observation, EmptyStateDefaults) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    auto obs = Observation::from_state(s);
    EXPECT_FLOAT_EQ(obs.enemy_count, 0);
    EXPECT_FLOAT_EQ(obs.nearest_enemy_dist, 99.0f);
}
