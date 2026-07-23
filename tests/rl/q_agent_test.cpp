// G8.4: QAgent tests
#include <gtest/gtest.h>
#include "ai/rl/q_agent.h"
#include "ai/rl/observation.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"

using namespace rl;
using namespace mcts;

TEST(QAgent, SelectReturnsValidAction) {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    QAgent qa(0.1, 0.9, 0.1);
    uint32_t seed = 42;
    auto action = qa.select(s, seed);
    EXPECT_GE((int)action, 0);
    EXPECT_LT((int)action, (int)CombatAction::COUNT);
}

TEST(QAgent, EmptyTableSize) {
    QAgent qa;
    EXPECT_EQ(qa.table_size(), 0u);
}

TEST(QAgent, UpdateIncreasesTable) {
    QAgent qa;
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.alive = true;
    auto obs = Observation::from_state(s);
    qa.update(obs, CombatAction::ATTACK, 10.0, obs);
    EXPECT_GT(qa.table_size(), 0u);
}

TEST(QAgent, QValueConverges) {
    QAgent qa(0.5, 0.9, 0.0); // epsilon=0, always greedy
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.alive = true;
    auto obs = Observation::from_state(s);

    // Repeated updates with positive reward should increase Q for attack
    for (int i = 0; i < 20; i++)
        qa.update(obs, CombatAction::ATTACK, 5.0, obs);

    uint32_t seed = 42;
    auto action = qa.select(s, seed);
    // With epsilon=0 and many positive updates to ATTACK, should select ATTACK
    EXPECT_EQ(action, CombatAction::ATTACK);
}

TEST(QAgent, EpsilonExploration) {
    QAgent qa(0.1, 0.9, 1.0); // epsilon=1.0, always explore
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.alive = true;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    uint32_t seed = 42;
    // With epsilon=1.0, should pick various actions over multiple calls
    std::vector<int> seen((int)CombatAction::COUNT, 0);
    for (int i = 0; i < 50; i++) {
        auto a = qa.select(s, seed);
        seen[(int)a]++;
    }
    int non_zero = 0;
    for (auto c : seen) if (c > 0) non_zero++;
    EXPECT_GT(non_zero, 1); // should explore at least 2 different actions
}

TEST(QAgent, Hyperparameters) {
    QAgent qa(0.1, 0.95, 0.2);
    EXPECT_DOUBLE_EQ(qa.alpha(), 0.1);
    EXPECT_DOUBLE_EQ(qa.gamma(), 0.95);
    EXPECT_DOUBLE_EQ(qa.epsilon(), 0.2);
}
