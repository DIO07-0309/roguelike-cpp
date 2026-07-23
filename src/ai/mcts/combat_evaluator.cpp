// G8.3: Combat evaluator implementation
#include "ai/mcts/combat_evaluator.h"

namespace mcts {

double evaluate_state(const SimulationState& state) {
    if (state.victory) return 1000.0;
    if (!state.player.alive) return -1000.0;

    double score = state.player.hp * 2.0;
    for (auto& m : state.monsters) {
        if (!m.alive) score += 200.0;
        else score -= m.hp * 1.5;
    }
    // Prefer faster wins
    score -= state.depth * 5.0;
    return score;
}

} // namespace mcts
