// G8.3: SimulationState unit tests
#include <gtest/gtest.h>
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"
#include "ai/mcts/combat_evaluator.h"

using namespace mcts;

TEST(SimState, CloneIndependent) {
    SimulationState a;
    a.player.hp = 80;
    a.player.x = 5;
    a.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    SimulationState b = a.clone();
    b.player.hp = 50;  // modify clone
    EXPECT_FLOAT_EQ(a.player.hp, 80); // original unchanged
    EXPECT_FLOAT_EQ(b.player.hp, 50);
}

TEST(SimState, AliveMonstersCount) {
    SimulationState s;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    s.monsters.push_back(MonsterSnapshot{"orc", 30, 30, 5, 0, 7, 3, 1, {}, false}); // dead
    EXPECT_EQ(s.alive_monsters(), 1);
}

TEST(SimState, TerminalOnPlayerDeath) {
    SimulationState s;
    s.player.alive = false;
    EXPECT_TRUE(s.is_terminal());
}

TEST(SimState, VictoryOnAllEnemiesDead) {
    SimulationState s;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, false});
    EXPECT_TRUE(s.alive_monsters() == 0);
}

TEST(ActionFilter, PossibleActionsNotEmpty) {
    SimulationState s;
    s.player.alive = true;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    auto actions = get_possible_actions(s);
    EXPECT_GE(actions.size(), 5u); // at least move + wait
}

TEST(ActionFilter, DeadPlayerYieldsEmpty) {
    SimulationState s;
    s.player.alive = false;
    auto actions = get_possible_actions(s);
    EXPECT_TRUE(actions.empty());
}

TEST(CombatEvaluator, VictoryHighScore) {
    SimulationState s;
    s.victory = true;
    EXPECT_GT(evaluate_state(s), 900.0);
}

TEST(CombatEvaluator, DeathLowScore) {
    SimulationState s;
    s.player.alive = false;
    EXPECT_LT(evaluate_state(s), -900.0);
}

TEST(CombatEvaluator, NeutralState) {
    SimulationState s;
    s.player.hp = 100;
    s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
    double score = evaluate_state(s);
    EXPECT_GT(score, 0);
    EXPECT_LT(score, 500); // alive but enemy alive too
}
