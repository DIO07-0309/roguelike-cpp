// G8.4: RandomAgent implementation
#include "ai/rl/random_agent.h"
#include <vector>

namespace rl {

mcts::CombatAction RandomAgent::select(const mcts::SimulationState& state, uint32_t& seed) {
    auto actions = mcts::get_possible_actions(state);
    if (actions.empty()) return mcts::CombatAction::WAIT;
    seed = seed * 1664525u + 1013904223u;
    return actions[seed % actions.size()];
}

} // namespace rl
