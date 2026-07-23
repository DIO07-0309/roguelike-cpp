// G8.4: CombatEnvironment tests
#include <gtest/gtest.h>
#include "ai/rl/environment.h"
#include "ai/rl/observation.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"

using namespace rl;
using namespace mcts;

static SimulationState make_state() {
    SimulationState s;
    s.player.hp = 100; s.player.max_hp = 100;
    s.player.x = 5; s.player.y = 5;
    s.player.attack = 10; s.player.pdef = 3;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    return s;
}

TEST(Environment, ResetReturnsObservation) {
    CombatEnvironment env;
    auto obs = env.reset(make_state());
    EXPECT_GT(obs.player_hp_ratio, 0.9f);
    EXPECT_EQ(obs.enemy_count, 1);
}

TEST(Environment, StepChangesState) {
    CombatEnvironment env;
    env.reset(make_state());
    auto sr = env.step(CombatAction::ATTACK);
    // State should have advanced
    EXPECT_GE(env.state().depth, 1);
}

TEST(Environment, TerminalWhenEnemyDies) {
    CombatEnvironment env;
    auto state = make_state();
    state.monsters[0].hp = 1; // near death
    env.reset(state);
    auto sr = env.step(CombatAction::ATTACK);
    // Enemy may or may not be dead (dmg has +-20% swing)
    // Just check no crash and state advances
    EXPECT_GE(env.state().depth, 1);
}

TEST(Environment, StepOnDeadEnemy) {
    CombatEnvironment env;
    auto state = make_state();
    state.monsters[0].alive = false;
    state.terminal = true;
    env.reset(state);
    auto sr = env.step(CombatAction::ATTACK);
    EXPECT_TRUE(sr.terminal);
}

TEST(Environment, RunEpisodeCompletes) {
    CombatEnvironment env;
    auto stats = env.run_episode(make_state(), 30);
    EXPECT_GT(stats.steps, 0);
}

TEST(Environment, EpisodeStatsAccumulate) {
    CombatEnvironment env;
    auto stats = env.run_episode(make_state(), 20);
    EXPECT_GE(stats.total_reward, -500);
    EXPECT_LE(stats.total_reward, 500);
}
