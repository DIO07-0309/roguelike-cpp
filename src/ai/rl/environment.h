#pragma once
// G8.4: CombatEnvironment — Gym-like API wrapping SimulationState.
// reset(state) → step(action) → {observation, reward, terminal}
// Zero GameScene/Renderer/Audio dependency.
// G8.5 can swap in a PyTorch agent using the same interface.

#include "observation.h"
#include "ai/mcts/simulation_state.h"
#include "ai/mcts/action.h"
#include <vector>

namespace rl {

struct StepResult {
    Observation observation;
    double reward = 0;
    bool terminal = false;
};

struct EpisodeStats {
    int steps = 0;
    double total_reward = 0;
    bool won = false;
};

class CombatEnvironment {
public:
    CombatEnvironment() = default;

    // Reset to a new initial state, return first observation
    Observation reset(const mcts::SimulationState& initial);

    // Execute one action, return observation + reward
    StepResult step(mcts::CombatAction action);

    // True when episode is over
    bool is_done() const { return _state.is_terminal(); }

    // Current state (read-only)
    const mcts::SimulationState& state() const { return _state; }

    // Run a full episode with random actions, return stats
    EpisodeStats run_episode(const mcts::SimulationState& initial, int max_steps = 50);

private:
    mcts::SimulationState _state;
    uint32_t _rng_seed = 42;
};

} // namespace rl
