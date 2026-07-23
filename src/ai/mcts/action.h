#pragma once
// G8.3: CombatAction — actions available during MCTS search
#include <vector>
#include <string>
#include <cmath>
#include "simulation_state.h"

namespace mcts {

enum class CombatAction {
    ATTACK,
    SKILL_1, SKILL_2, SKILL_3, SKILL_4,
    MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT,
    WAIT,
    COUNT
};

inline const char* action_name(CombatAction a) {
    switch (a) {
    case CombatAction::ATTACK:    return "attack";
    case CombatAction::SKILL_1:   return "skill_1";
    case CombatAction::SKILL_2:   return "skill_2";
    case CombatAction::SKILL_3:   return "skill_3";
    case CombatAction::SKILL_4:   return "skill_4";
    case CombatAction::MOVE_UP:   return "move_up";
    case CombatAction::MOVE_DOWN: return "move_down";
    case CombatAction::MOVE_LEFT: return "move_left";
    case CombatAction::MOVE_RIGHT:return "move_right";
    default:                      return "wait";
    }
}

// All 9 possible actions
inline std::vector<CombatAction> all_actions() {
    return {CombatAction::ATTACK, CombatAction::SKILL_1, CombatAction::SKILL_2,
            CombatAction::SKILL_3, CombatAction::SKILL_4,
            CombatAction::MOVE_UP, CombatAction::MOVE_DOWN,
            CombatAction::MOVE_LEFT, CombatAction::MOVE_RIGHT};
}

// Filter: return only currently legal actions
inline std::vector<CombatAction> get_possible_actions(const SimulationState& state) {
    std::vector<CombatAction> actions;
    auto& p = state.player;
    if (!p.alive) return actions;

    // Attack always possible if enemy in range
    bool enemy_in_range = false;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d < 2.0f) { enemy_in_range = true; break; }
    }
    if (enemy_in_range && p.attack_cooldown <= 0)
        actions.push_back(CombatAction::ATTACK);

    // Skills: on cooldown? skip
    for (int i = 0; i < 4; i++)
        if (p.skill_cooldowns[i] <= 0)
            actions.push_back((CombatAction)((int)CombatAction::SKILL_1 + i));

    // Movement (manhattan is cheaper)
    float mdx[4] = {0, 0, -1, 1}, mdy[4] = {-1, 1, 0, 0};
    for (int d = 0; d < 4; d++) {
        float nx = p.x + mdx[d], ny = p.y + mdy[d];
        if (nx >= 0 && nx < 40 && ny >= 0 && ny < 30)
            actions.push_back((CombatAction)((int)CombatAction::MOVE_UP + d));
    }

    // Always can wait
    actions.push_back(CombatAction::WAIT);
    return actions;
}

} // namespace mcts
