#pragma once
// G8.3: Combat evaluator — standalone state scoring for MCTS rollouts.
// Also usable by DecisionAgent or BT for action comparison.

#include "simulation_state.h"

namespace mcts {

// Score a terminal or heuristically-evaluated state.
// Higher = better for player.
double evaluate_state(const SimulationState& state);

} // namespace mcts
