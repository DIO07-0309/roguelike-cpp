// G8.4: QAgent — tabular Q-learning implementation
#include "ai/rl/q_agent.h"
#include <limits>
#include <algorithm>

namespace rl {

std::string QAgent::_make_key(const std::string& obs_key, int action_id) const {
    return obs_key + "|" + std::to_string(action_id);
}

double QAgent::_q_value(const std::string& obs_key, int action_id) const {
    auto it = _q.find(_make_key(obs_key, action_id));
    return it != _q.end() ? it->second : 0.0;
}

mcts::CombatAction QAgent::_best_action(const std::string& obs_key) const {
    mcts::CombatAction best = mcts::CombatAction::ATTACK;
    double best_q = -std::numeric_limits<double>::max();
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
        double qv = _q_value(obs_key, i);
        if (qv > best_q) { best_q = qv; best = (mcts::CombatAction)i; }
    }
    return best;
}

mcts::CombatAction QAgent::select(const mcts::SimulationState& state, uint32_t& seed) {
    auto actions = mcts::get_possible_actions(state);
    if (actions.empty()) return mcts::CombatAction::WAIT;

    std::string obs_key = Observation::from_state(state).to_key();

    // Epsilon-greedy
    seed = seed * 1664525u + 1013904223u;
    double roll = (double)(seed & 0x7FFFFFFF) / 2147483648.0;
    if (roll < _epsilon) {
        // Explore: random action
        return actions[seed % actions.size()];
    }

    // Exploit: best known Q-value among possible actions
    mcts::CombatAction best = actions[0];
    double best_q = -std::numeric_limits<double>::max();
    for (auto a : actions) {
        double qv = _q_value(obs_key, (int)a);
        if (qv > best_q) { best_q = qv; best = a; }
    }
    return best;
}

void QAgent::update(const Observation& obs, mcts::CombatAction action,
                     double reward, const Observation& next_obs) {
    std::string s_key = obs.to_key();
    std::string sn_key = next_obs.to_key();

    double old_q = _q_value(s_key, (int)action);
    double max_next = -std::numeric_limits<double>::max();
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
        double qv = _q_value(sn_key, i);
        if (qv > max_next) max_next = qv;
    }
    if (max_next < -999) max_next = 0; // terminal state

    double new_q = old_q + _alpha * (reward + _gamma * max_next - old_q);
    _q[_make_key(s_key, (int)action)] = new_q;
}

std::vector<QActionStats> QAgent::action_distribution() const {
    std::vector<QActionStats> stats;
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
        QActionStats s;
        s.action_name = mcts::action_name((mcts::CombatAction)i);
        s.count = 0; s.avg_q = 0;
        double sum_q = 0;
        for (auto& [key, qv] : _q) {
            if (key.find("|" + std::to_string(i)) != std::string::npos) {
                s.count++; sum_q += qv;
            }
        }
        if (s.count > 0) s.avg_q = sum_q / s.count;
        stats.push_back(s);
    }
    return stats;
}

} // namespace rl
