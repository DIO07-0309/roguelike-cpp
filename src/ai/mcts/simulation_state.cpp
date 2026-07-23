// G8.3: SimulationState implementation
#include "ai/mcts/simulation_state.h"

namespace mcts {

uint32_t RNGState::next() const {
    // Simple multiplicative LCG for deterministic replay
    uint32_t s = seed + calls * 1103515245u + 12345u;
    s = s * 1664525u + 1013904223u;
    return s;
}

SimulationState SimulationState::clone() const {
    SimulationState s;
    s.player = player;
    s.player.buffs = player.buffs; // deep copy vector
    s.monsters = monsters;
    s.rng = rng;
    s.depth = depth;
    s.terminal = terminal;
    s.victory = victory;
    return s;
}

int SimulationState::alive_monsters() const {
    int n = 0;
    for (auto& m : monsters) if (m.alive) n++;
    return n;
}

} // namespace mcts
