// G8.4: RL standalone runner — called from main.cpp before engine init.
// Runs CombatEnvironment + RandomAgent/QAgent without GameScene.
#include "ai/rl/environment.h"
#include "ai/rl/random_agent.h"
#include "ai/rl/q_agent.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"
#include <cstdio>

void run_rl_mode(int test_episodes, int train_episodes) {
    using namespace rl;
    using namespace mcts;

    // Build a typical combat scenario
    auto make_scenario = []() -> SimulationState {
        SimulationState s;
        s.player.hp = 100; s.player.max_hp = 100;
        s.player.x = 5; s.player.y = 5;
        s.player.attack = 10; s.player.pdef = 3; s.player.mdef = 1;
        s.player.alive = true;
        s.player.skill_cooldowns[0] = 0; s.player.skill_cooldowns[1] = 0;
        s.monsters.push_back(MonsterSnapshot{"slime", 20, 20, 3, 0, 3, 1, 0, {}, true});
        s.monsters.push_back(MonsterSnapshot{"orc", 30, 30, 5, 0, 7, 3, 1, {}, true});
        return s;
    };

    CombatEnvironment env;
    uint32_t seed = 42;

    // ── Test mode: RandomAgent baseline ──
    if (test_episodes > 0) {
        RandomAgent random_agent;
        printf("═══ RL TEST: %d episodes (RandomAgent) ═══\n", test_episodes);
        int wins = 0; double total_reward = 0; int total_steps = 0;
        for (int i = 0; i < test_episodes; i++) {
            auto initial = make_scenario();
            initial.rng.seed = seed + i * 7919u;
            env.reset(initial);
            int steps = 0;
            while (!env.is_done() && steps < 50) {
                uint32_t local_seed = seed + i * 7331u + steps;
                auto a = random_agent.select(env.state(), local_seed);
                auto sr = env.step(a);
                total_reward += sr.reward;
                steps++;
            }
            if (env.state().victory) wins++;
            total_steps += steps;
            if (test_episodes >= 10 && (i+1) % (test_episodes/10) == 0)
                printf("  [%d/%d] wins=%d avg_r=%.1f\n", i+1, test_episodes, wins, total_reward/(i+1));
        }
        printf("RandomAgent: %d/%d wins (%.1f%%), avg reward %.1f, avg steps %.1f\n",
               wins, test_episodes, wins*100.0/test_episodes,
               total_reward/test_episodes, (double)total_steps/test_episodes);
    }

    // ── Train mode: Q-learning ──
    if (train_episodes > 0) {
        QAgent q_agent(0.1, 0.9, 0.1);
        printf("═══ RL TRAIN: %d episodes (QAgent) ═══\n", train_episodes);
        int wins = 0;
        for (int i = 0; i < train_episodes; i++) {
            auto initial = make_scenario();
            initial.rng.seed = seed + i * 7919u;
            auto obs = env.reset(initial);
            while (!env.is_done()) {
                uint32_t local_seed = seed + i * 7331u + env.state().depth;
                auto a = q_agent.select(env.state(), local_seed);
                auto sr = env.step(a);
                q_agent.update(obs, a, sr.reward, sr.observation);
                obs = sr.observation;
            }
            if (env.state().victory) wins++;
            if (train_episodes >= 10 && (i+1) % (train_episodes/10) == 0)
                printf("  [%d/%d] wins=%d Q-table=%zu\n", i+1, train_episodes, wins, q_agent.table_size());
        }
        printf("QAgent after training: %d/%d wins (%.1f%%), Q-table size=%zu\n",
               wins, train_episodes, wins*100.0/train_episodes, q_agent.table_size());
        auto dist = q_agent.action_distribution();
        printf("Action Q-values:\n");
        for (auto& d : dist)
            if (d.count > 0)
                printf("  %10s: %5d entries  avg_q=%.2f\n",
                       d.action_name.c_str(), d.count, d.avg_q);
    }
}
