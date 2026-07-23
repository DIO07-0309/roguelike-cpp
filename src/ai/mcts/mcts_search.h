#pragma once
// G8.3: MCTS search engine — Combat Action Search
#include "simulation_state.h"
#include "action.h"
#include "mcts_node.h"
#include <memory>
#include <random>

namespace mcts {

// ── Combat state simulator (rollout) ──────────────────────
namespace detail {
    static void apply_action(SimulationState& state, CombatAction a, uint32_t& rng_seed);
    static double evaluate_terminal(const SimulationState& state);
}

class MCTS {
public:
    explicit MCTS(int iterations = 100) : _iterations(iterations) {}

    CombatAction search(const SimulationState& state);

private:
    int _iterations;

    MCTSNode* select(MCTSNode* node);
    void expand(MCTSNode* node);
    double simulate(const SimulationState& state, uint32_t rng_seed);
    void backpropagate(MCTSNode* leaf, double reward);
};

} // namespace mcts
