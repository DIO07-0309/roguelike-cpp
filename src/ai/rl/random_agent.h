#pragma once
// G8.4: RandomAgent — baseline agent that picks actions uniformly at random.
// Used as the lower bound for comparing learned agents.

#include "ai/mcts/action.h"
#include "ai/mcts/simulation_state.h"

namespace rl {

class RandomAgent {
public:
    // Select a random action from possible actions
    mcts::CombatAction select(const mcts::SimulationState& state, uint32_t& seed);
};

} // namespace rl
