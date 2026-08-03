// G8.4/F15.4: RL standalone runner — called from main.cpp before engine init.
// Runs CombatEnvironment + RandomAgent/QAgent without GameScene.
// F15.4: run_rl_mirror_mode() trains QAgent against 4 player styles.
#include "ai/rl/environment.h"
#include "ai/rl/random_agent.h"
#include "ai/rl/q_agent.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"
#include "ai/mirror/mirror_agent.h"
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

// ── F15.4: Mirror self-play training ──
void run_rl_mirror_mode(int episodes) {
    using namespace rl;
    using namespace mcts;

    // Four player styles to train against
    PlayerHabitProfile profiles[4];
    // AGGRESSIVE: high attack, low dodge
    profiles[0].attack_frequency = 0.90f; profiles[0].dodge_rate = 0.08f;
    profiles[0].aggression_score = 0.95f; profiles[0].style = PlayerStyle::AGGRESSIVE;
    profiles[0].predict_attack_heavy = true;
    // DEFENSIVE: high dodge, low attack
    profiles[1].attack_frequency = 0.25f; profiles[1].dodge_rate = 0.45f;
    profiles[1].aggression_score = 0.35f; profiles[1].style = PlayerStyle::DEFENSIVE;
    profiles[1].predict_low_dodge = false;
    // SNIPER: retreat-heavy, range attacks
    profiles[2].attack_frequency = 0.55f; profiles[2].retreat_rate = 0.40f;
    profiles[2].style = PlayerStyle::SNIPER;
    // MAGE: skill-heavy
    profiles[2].skill_frequency = 0.25f; profiles[2].predict_skill_spam = true;
    profiles[2].predicted_fav_skill = 1;  // fireball
    // BALANCED (default)
    profiles[3] = PlayerHabitProfile{};

    auto make_scenario_with_style = [](int style_id) -> SimulationState {
        SimulationState s;
        s.player.hp = 100; s.player.max_hp = 100;
        s.player.x = 5; s.player.y = 5;
        s.player.attack = 10; s.player.pdef = 3; s.player.mdef = 1;
        s.player.alive = true;
        s.monsters.push_back(MonsterSnapshot{"mirror_boss", 200, 200, 3, 0, 15, 5, 5, {}, true, true});
        s.player_style = style_id + 1; // 1-4
        return s;
    };

    CombatEnvironment env;
    uint32_t seed = 42;

    printf("═══ RL MIRROR: %d episodes × 4 styles ═══\n", episodes);
    for (int style_i = 0; style_i < 4; style_i++) {
        QAgent q_agent(0.15, 0.9, 0.12);
        int wins = 0; double total_r = 0;
        const auto& profile = profiles[style_i];

        for (int i = 0; i < episodes; i++) {
            auto initial = make_scenario_with_style(style_i);
            initial.rng.seed = seed + style_i * 7919u + i * 7331u;
            auto obs = env.reset(initial);
            while (!env.is_done()) {
                uint32_t local_seed = seed + style_i * 7331u + i * 7919u + env.state().depth;
                auto a = q_agent.select(env.state(), local_seed);
                auto sr = env.step(a);
                // Mirror reward replaces default step reward
                double base = sr.reward;
                bool dodged = (a == CombatAction::MOVE_LEFT || a == CombatAction::MOVE_RIGHT
                            || a == CombatAction::MOVE_UP || a == CombatAction::MOVE_DOWN);
                double mirror_bonus = MirrorAgent::mirror_reward(
                    profile, (int)a, base, 0, dodged, false);
                q_agent.update(obs, a, base + mirror_bonus, sr.observation);
                obs = sr.observation;
                total_r += base + mirror_bonus;
            }
            if (env.state().victory) wins++;
        }
        printf("  [%s] %d/%d wins (%.1f%%), avg_r=%.1f, Q=%zu\n",
            profiles[style_i].style_name(),
            wins, episodes, wins*100.0/episodes,
            total_r/episodes, q_agent.table_size());
    }
    printf("Mirror RL complete.\n");
}
