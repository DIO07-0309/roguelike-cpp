#pragma once
#include <array>
#include <unordered_map>
#include <vector>
#include "ai/player_behavior/player_action.h"

// M4.4: TacticalChainTable — 玩家战术链序列学习 (n-gram, 无神经网络)
// 从 PlayerAction 流提取战术符号序列 → 3-gram 计数 → 预测下一步战术动作
// 降级链: 3-gram → 2-gram → 克隆表 → 规则 (集成方负责后两级)

enum class TacticalSymbol : int {
    SKILL_0 = 0, SKILL_1, SKILL_2, SKILL_3,   // 技能 (skill_id 0-3)
    MOVE_L = 4, MOVE_R, MOVE_U, MOVE_D,       // 位移 (value 0-3)
    COMBO_1 = 8, COMBO_2, COMBO_3,            // 连招段 (combo_stage 0/1/2)
    COUNT = 11
};

inline int symbol_from_action(const PlayerAction& a) {
    switch (a.type) {
    case PlayerActionType::SKILL:
        if (a.skill_id >= 0 && a.skill_id <= 3) return (int)TacticalSymbol::SKILL_0 + a.skill_id;
        return -1;
    case PlayerActionType::MOVE:
        if (a.value >= 0 && a.value <= 3) return (int)TacticalSymbol::MOVE_L + a.value;
        return -1;
    case PlayerActionType::ATTACK:
        if (a.combo_stage >= 0 && a.combo_stage <= 2)
            return (int)TacticalSymbol::COMBO_1 + a.combo_stage;
        return -1;   // 未知连招段 → 非战术符号
    default:
        return -1;   // 非战术动作 (DODGE/HEAL/TAKE_DAMAGE 等) 不参与战术链
    }
}

struct ChainPrediction {
    int best = -1;         // 预测的 TacticalSymbol
    float confidence = 0.0f;
    int level = -1;        // 0=3gram, 1=2gram, -1=miss
};

class TacticalChainTable {
public:
    void clear();
    void build(const std::vector<PlayerAction>& stream);
    void build_triple(int s0, int s1, int s2);
    ChainPrediction predict(int s0, int s1) const;
    ChainPrediction predict_fuzzy2(int s0) const;
    size_t entries() const { return _table3.size(); }

private:
    std::unordered_map<int, int> _table3;      // key = s0*121 + s1*11 + s2
    int _prefix_total[121] = {};               // 2-前缀 总次数 (分母)
    int _s0_total[11] = {};                  // 单前缀 总次数 (2-gram 分母)
    static int _key3(int s0, int s1, int s2) { return s0 * 121 + s1 * 11 + s2; }
};