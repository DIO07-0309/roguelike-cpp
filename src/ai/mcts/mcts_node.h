#pragma once
// G8.3: MCTSNode — tree node for Monte Carlo Tree Search
#include <vector>
#include <memory>
#include <cmath>
#include "simulation_state.h"
#include "action.h"

namespace mcts {

struct MCTSNode {
    SimulationState state;
    CombatAction action = CombatAction::WAIT;
    MCTSNode* parent = nullptr;
    std::vector<std::unique_ptr<MCTSNode>> children;
    int visits = 0;
    double reward = 0.0;

    // UCT: Upper Confidence Bound for Trees
    double uct_value(double C = 1.414) const {
        if (visits == 0) return 1e9; // prioritize unvisited
        double exploitation = reward / (double)visits;
        double exploration  = C * std::sqrt(std::log((double)(parent ? parent->visits : 1)) / visits);
        return exploitation + exploration;
    }

    bool is_fully_expanded() const {
        return !children.empty(); // simplified: one child = one action tried
    }

    bool is_terminal() const {
        return state.is_terminal();
    }
};

} // namespace mcts
