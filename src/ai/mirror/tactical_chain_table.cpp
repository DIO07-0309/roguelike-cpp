#include "ai/mirror/tactical_chain_table.h"

void TacticalChainTable::clear() {
    _table3.clear();
    for (auto& x : _prefix_total) x = 0;
    for (auto& x : _s0_total) x = 0;
}

void TacticalChainTable::build_triple(int s0, int s1, int s2) {
    if (s0 < 0 || s1 < 0 || s2 < 0) return;
    if (s0 >= 11 || s1 >= 11 || s2 >= 11) return;
    _table3[_key3(s0, s1, s2)]++;
    _prefix_total[s0 * 11 + s1]++;
    _s0_total[s0]++;
}

void TacticalChainTable::build(const std::vector<PlayerAction>& stream) {
    std::vector<int> syms;
    for (const auto& a : stream) {
        int s = symbol_from_action(a);
        if (s >= 0) syms.push_back(s);
    }
    for (size_t i = 0; i + 2 < syms.size(); i++)
        build_triple(syms[i], syms[i + 1], syms[i + 2]);
}

ChainPrediction TacticalChainTable::predict(int s0, int s1) const {
    ChainPrediction p;
    if (s0 < 0 || s1 < 0 || s0 >= 11 || s1 >= 11) return p;
    int base = s0 * 121 + s1 * 11;
    int total = _prefix_total[s0 * 11 + s1];
    if (total < 3) return predict_fuzzy2(s1);     // 样本不足 → 自动降级 2-gram
    int best_n = 0;
    for (int s2 = 0; s2 < 11; s2++) {
        int n = 0;
        auto it = _table3.find(base + s2);
        if (it != _table3.end()) n = it->second;
        if (n > best_n) { best_n = n; p.best = s2; }
    }
    if (best_n <= 0) return predict_fuzzy2(s1);
    p.confidence = (float)best_n / (float)total;
    p.level = 0;
    return p;
}

ChainPrediction TacticalChainTable::predict_fuzzy2(int s0) const {
    ChainPrediction p;
    if (s0 < 0 || s0 >= 11) return p;
    if (_s0_total[s0] < 5) return p;
    int best_s1 = -1, best_n = 0;
    for (int s1 = 0; s1 < 11; s1++) {
        int n = _prefix_total[s0 * 11 + s1];
        if (n > best_n) { best_n = n; best_s1 = s1; }
    }
    if (best_s1 < 0) return p;
    p.best = best_s1;                           // 2-gram: 该前缀最常跟的符号
    p.confidence = (float)best_n / (float)_s0_total[s0];
    p.level = 1;
    return p;
}