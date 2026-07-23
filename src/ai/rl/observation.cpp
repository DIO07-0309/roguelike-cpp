// G8.4: Observation implementation
#include "ai/rl/observation.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace rl {

Observation Observation::from_state(const mcts::SimulationState& state) {
    Observation obs;
    auto& p = state.player;
    obs.player_hp_ratio = p.max_hp > 0 ? p.hp / p.max_hp : 0;
    obs.player_attack = (float)p.attack;

    int alive = 0;
    float nearest = 999;
    float strongest = 0;
    bool has_boss = false;
    for (auto& m : state.monsters) {
        if (!m.alive) continue;
        alive++;
        float d = std::abs(m.x - p.x) + std::abs(m.y - p.y);
        if (d < nearest) nearest = d;
        float m_hp_ratio = m.max_hp > 0 ? m.hp / m.max_hp : 0;
        if (m_hp_ratio > strongest) strongest = m_hp_ratio;
        if (m.is_boss) has_boss = true;
    }
    obs.enemy_count = (float)alive;
    obs.nearest_enemy_dist = nearest;
    obs.strongest_hp_ratio = strongest;
    obs.boss_present = has_boss ? 1.0f : 0;
    obs.buff_count = (float)p.buffs.size();
    return obs;
}

std::vector<float> Observation::to_vector() const {
    return {player_hp_ratio, player_attack, enemy_count, nearest_enemy_dist,
            strongest_hp_ratio, boss_present, buff_count};
}

std::string Observation::to_key() const {
    // Discretize continuous values into buckets for Q-table
    int hp_b = (int)(player_hp_ratio * 10);           // 0-10
    int atk_b = (int)(player_attack / 5.0f);          // 0-6
    int ec_b = (int)enemy_count;                       // 0-5
    int nd_b = (int)(nearest_enemy_dist / 2.0f);      // 0-10
    int boss_b = (int)boss_present;
    // Compact key
    std::ostringstream ss;
    ss << hp_b << ":" << atk_b << ":" << ec_b << ":" << nd_b << ":" << boss_b;
    return ss.str();
}

} // namespace rl
