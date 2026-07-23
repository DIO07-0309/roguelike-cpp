// G8.4: CombatEnvironment implementation
#include "ai/rl/environment.h"
#include "ai/mcts/combat_evaluator.h"
#include <cstdlib>

namespace rl {

Observation CombatEnvironment::reset(const mcts::SimulationState& initial) {
    _state = initial.clone();
    _rng_seed = _state.rng.seed;
    return Observation::from_state(_state);
}

StepResult CombatEnvironment::step(mcts::CombatAction action) {
    if (_state.is_terminal()) {
        return {Observation::from_state(_state), 0, true};
    }

    // Apply action using same combat model as MCTS
    _state.depth++;
    auto& p = _state.player;

    auto lcg = [this]() -> float {
        _rng_seed = _rng_seed * 1664525u + 1013904223u;
        return (float)(_rng_seed & 0x7FFFFFFF) / 2147483648.0f;
    };

    float prev_hp = p.hp;
    int prev_alive = 0;
    for (auto& m : _state.monsters) if (m.alive) prev_alive++;

    switch (action) {
    case mcts::CombatAction::ATTACK: {
        float best_d = 999;
        mcts::MonsterSnapshot* best = nullptr;
        for (auto& m : _state.monsters) {
            if (!m.alive) continue;
            float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
            if (d < 1.5f && d < best_d) { best_d = d; best = &m; }
        }
        if (best) {
            int base = p.attack - (int)(best->pdef * 0.5f);
            int dmg = std::max(1, (int)(base * (0.8f + 0.4f * lcg())));
            best->hp -= (float)dmg;
            if (best->hp <= 0) best->alive = false;
        }
        p.attack_cooldown = 0.5f;
        break;
    }
    case mcts::CombatAction::SKILL_1:
    case mcts::CombatAction::SKILL_2:
    case mcts::CombatAction::SKILL_3:
    case mcts::CombatAction::SKILL_4: {
        int si = (int)action - (int)mcts::CombatAction::SKILL_1;
        for (auto& m : _state.monsters) {
            if (!m.alive) continue;
            float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
            if (d < 3.0f) {
                int base = (int)(p.attack * 1.5f) - (int)(m.pdef * 0.5f);
                int dmg = std::max(1, (int)(base * (0.8f + 0.4f * lcg())));
                m.hp -= (float)dmg;
                if (m.hp <= 0) m.alive = false;
            }
        }
        p.skill_cooldowns[si] = 3.0f;
        break;
    }
    case mcts::CombatAction::MOVE_UP:    p.y -= 1; break;
    case mcts::CombatAction::MOVE_DOWN:  p.y += 1; break;
    case mcts::CombatAction::MOVE_LEFT:  p.x -= 1; break;
    case mcts::CombatAction::MOVE_RIGHT: p.x += 1; break;
    default: break; // WAIT
    }

    // Cooldown decay
    if (p.attack_cooldown > 0) p.attack_cooldown -= 0.25f;
    for (int i = 0; i < 4; i++)
        if (p.skill_cooldowns[i] > 0) p.skill_cooldowns[i] -= 0.25f;

    // Monster retaliation
    for (auto& m : _state.monsters) {
        if (!m.alive) continue;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d < 1.5f && lcg() < 0.4f) {
            int dmg = std::max(1, m.attack - (int)(p.pdef * 0.5f));
            p.hp -= (float)dmg;
            if (p.hp <= 0) p.alive = false;
        }
    }

    // Terminal check
    bool any_alive = false;
    for (auto& m : _state.monsters) if (m.alive) { any_alive = true; break; }
    if (!any_alive) { _state.victory = true; _state.terminal = true; }
    if (!p.alive) _state.terminal = true;

    // Reward: damage dealt - damage taken
    double reward = 0;
    // Damage dealt
    for (auto& m : _state.monsters)
        if (!m.alive) reward += 50.0;
    // HP change
    float hp_delta = p.hp - prev_hp;
    if (hp_delta < 0) reward += hp_delta; // penalty for taking damage
    // Terminal bonuses
    if (_state.victory) reward += 200.0;
    if (!p.alive) reward -= 200.0;

    return {Observation::from_state(_state), reward, _state.terminal};
}

EpisodeStats CombatEnvironment::run_episode(const mcts::SimulationState& initial, int max_steps) {
    EpisodeStats ep;
    reset(initial);
    while (!is_done() && ep.steps < max_steps) {
        auto actions = mcts::get_possible_actions(_state);
        if (actions.empty()) break;
        mcts::CombatAction a = actions[_rng_seed % actions.size()];
        _rng_seed = _rng_seed * 1664525u + 1013904223u;
        auto sr = step(a);
        ep.total_reward += sr.reward;
        ep.steps++;
    }
    ep.won = _state.victory;
    return ep;
}

} // namespace rl
