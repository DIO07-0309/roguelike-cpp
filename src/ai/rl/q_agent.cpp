// G8.4: QAgent — tabular Q-learning implementation
#include "ai/rl/q_agent.h"
#include <limits>
#include <algorithm>
#include <cstdio>
#include "nlohmann/json.hpp"

#ifdef _WIN32
#include <direct.h>
#define rl_mkdir_impl(p) _mkdir(p)
#else
#include <sys/stat.h>
#define rl_mkdir_impl(p) mkdir(p, 0755)
#endif

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

int QAgent::exploit_action(const Observation& obs) const {
    std::string key = obs.to_key();
    bool known = false;
    for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
        if (_q.find(_make_key(key, i)) != _q.end()) { known = true; break; }
    }
    if (!known) return -1;   // 未见过该状态 → 不下接管 (镜像仲裁链落下级层)
    return (int)_best_action(key);
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
                     double reward, const Observation& next_obs, bool done) {
    std::string s_key = obs.to_key();
    std::string sn_key = next_obs.to_key();

    double old_q = _q_value(s_key, (int)action);
    double max_next = 0.0;
    if (!done) {
        // Q3.15 (B1 fix): 终局转移不得自举 — target = reward
        max_next = -std::numeric_limits<double>::max();
        for (int i = 0; i < (int)mcts::CombatAction::COUNT; i++) {
            double qv = _q_value(sn_key, i);
            if (qv > max_next) max_next = qv;
        }
        if (max_next < -999) max_next = 0; // next state unseen → treat as terminal
    }

    // Q3.15 (B2 fix): 学习率按访问次数衰减 — 表格 Q 收敛的必要条件
    std::string sa_key = _make_key(s_key, (int)action);
    int visits = _sa_visits[sa_key]++;
    double alpha_eff = _alpha / (1.0 + 0.05 * visits);

    double new_q = old_q + alpha_eff * (reward + _gamma * max_next - old_q);
    _q[sa_key] = new_q;
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

bool QAgent::save(const std::string& path) const {
    std::string dir = path;
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos);
        if (!dir.empty()) rl_mkdir_impl(dir.c_str());
    }
    nlohmann::json j;
    for (auto& [key, qv] : _q) j["q"][key] = qv;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fputs(j.dump(1).c_str(), f);
    fclose(f);
    return true;
}

bool QAgent::load(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
    fclose(f);
    try {
        auto j = nlohmann::json::parse(text);
        if (!j.contains("q") || !j["q"].is_object()) return false;
        _q.clear();
        for (auto it = j["q"].begin(); it != j["q"].end(); ++it)
            _q[it.key()] = it.value().get<double>();
    } catch (...) { return false; }
    return true;
}

} // namespace rl
