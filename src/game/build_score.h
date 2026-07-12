#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include "build_tag.h"

class Player;

// ============================================================
// D3 Step4: Build Score — 构筑评分系统
// ============================================================

// 构筑流派类型
enum class BuildType {
    NONE,           // 未成型
    BERSERKER,      // 狂战士: MELEE+COMBO+HEAVY
    FIRE_MAGE,      // 火法师: FIRE+AOE+DOT
    POISON_MASTER,  // 毒术大师: POISON+DOT
    TIME_MASTER,    // 时间术士: TIME+SUPPORT
    SUPPORT,        // 辅助者: HEAL+SUPPORT+DEFENSE
    PROJECTILE,     // 弹幕射手: PROJECTILE+RANGED
};

struct BuildScore {
    int scores[(int)BuildTag::COUNT] = {0};
    int total = 0;

    void add(BuildTag t, int v = 1) { int i = (int)t; scores[i] += v; total += v; }
    int  get(BuildTag t) const      { return scores[(int)t]; }

    // 判定最强流派
    BuildType identify() const;

    // 最强流派的名称 (中文)
    const char* build_name() const;
    const char* build_name_cn(BuildType bt) const;

    // 流派进度 0.0~1.0
    float progress(BuildType bt) const;
};

// 计算玩家当前构筑评分
BuildScore calculate_build(const Player* player);

// 根据 BuildScore 推荐 relic (id, weight) — 越高越优先
void recommend_relic_weights(const BuildScore& bs,
                              std::vector<std::string>& out_ids,
                              std::vector<int>& out_weights);

// 根据 BuildScore 判断是否与 relic 匹配
bool relic_matches_build(const BuildScore& bs, const std::string& relic_id);

// Build 奖励 (玩法变化, 非数值)
struct BuildBonus {
    float poison_duration_boost = 0.0f;
    float poison_tick_speed    = 0.0f;
    bool  heavy_heal           = false;
    bool  heavy_keep_combo     = false;
    float time_duration_boost  = 0.0f;
    float fire_extra_dot       = 0.0f;
};

BuildBonus get_build_bonus(BuildType bt);
