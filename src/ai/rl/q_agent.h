#pragma once
// G8.4: QAgent — tabular Q-learning agent.
// Uses discretized Observation→state_key for Q-table lookup.
// select(): epsilon-greedy from Q-table.
// update(): standard Q-learning update rule.

#include "observation.h"
#include "ai/mcts/action.h"
#include "ai/mcts/simulation_state.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace rl {

struct QActionStats {
    std::string action_name;
    int count = 0;
    double avg_q = 0;
};

class QAgent {
public:
    QAgent(double alpha = 0.1, double gamma = 0.9, double epsilon = 0.1)
        : _alpha(alpha), _gamma(gamma), _epsilon(epsilon) {}

    // Epsilon-greedy action selection
    mcts::CombatAction select(const mcts::SimulationState& state, uint32_t& seed);

    // Q(s,a) ← Q(s,a) + α * (reward + γ*max_a' Q(s',a') - Q(s,a))
    // Q3.15: done=true 时 target 只等于 reward — 终局不得自举未来价值 (B1 fix)
    // Q3.15: 有效学习率按 (s,a) 访问次数衰减 α/(1+0.05n) — 满足 Σα²<∞ 收敛条件 (B2 fix)
    void update(const Observation& obs, mcts::CombatAction action,
                double reward, const Observation& next_obs, bool done = false);

    // Stats
    size_t table_size() const { return _q.size(); }
    std::vector<QActionStats> action_distribution() const;

    // Q 表持久化 (JSON, 训练结果落盘供下次续训)
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    // v0.9.30: 纯 exploit 决策 (镜像运行时用, 无需 SimulationState)
    int exploit_action(const Observation& obs) const;

    // Hyperparameters
    double alpha() const { return _alpha; }
    double gamma() const { return _gamma; }
    double epsilon() const { return _epsilon; }
    void set_epsilon(double e) { _epsilon = e; }  // v0.9.29: 训练退火

private:
    std::string _make_key(const std::string& obs_key, int action_id) const;
    double _q_value(const std::string& obs_key, int action_id) const;
    mcts::CombatAction _best_action(const std::string& obs_key) const;

    std::unordered_map<std::string, double> _q;  // key:"obs:action" → q-value
    std::unordered_map<std::string, int> _sa_visits;  // Q3.15: (s,a) 访问计数 → 学习率衰减
    double _alpha, _gamma, _epsilon;
};

} // namespace rl
