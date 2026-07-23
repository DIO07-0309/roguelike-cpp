#pragma once
// G8.4: Observation — fixed-length feature vector from SimulationState.
// Used by: QAgent (key hashing), RL DQN/PPO (neural network input).

#include <vector>
#include <string>
#include <cstdint>
#include "ai/mcts/simulation_state.h"

namespace rl {

struct Observation {
    float player_hp_ratio = 1.0f;      // 0.0-1.0
    float player_attack = 10.0f;       // raw attack value
    float enemy_count = 0;             // number of alive enemies
    float nearest_enemy_dist = 99.0f;  // Manhattan distance to nearest
    float strongest_hp_ratio = 0;      // strongest enemy's HP ratio
    float boss_present = 0;            // 0 or 1
    float buff_count = 0;              // number of active buffs

    // Convert to flat float vector (for neural net input / hash key)
    std::vector<float> to_vector() const;

    // Hash key for Q-table lookup
    std::string to_key() const;

    static Observation from_state(const mcts::SimulationState& state);
};

} // namespace rl
