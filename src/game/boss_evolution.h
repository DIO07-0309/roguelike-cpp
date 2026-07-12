#pragma once
#include <string>
#include <vector>
#include "world_state.h"
#include "build_score.h"

// ============================================================
// D5 Step2: Boss Evolution — Adaptive Boss AI & Skill Variants
// ============================================================

struct BossSkillMod {
    float cooldown_scale = 1.0f;    // 技能CD倍率
    float damage_scale = 1.0f;      // 伤害倍率
    float area_scale = 1.0f;        // 范围倍率
    int   extra_projectiles = 0;    // 额外弹幕数
    bool  double_strike = false;    // 双倍打击
    const char* variant_name = nullptr; // "Triple Slash" etc.
};

struct BossEvolutionData {
    int   boss_floor = 0;
    bool  last_stand_triggered = false;   // HP<15%
    bool  build_counter_active[6] = {false}; // 6种BuildType反制
    BossSkillMod charge_mod;       // 冲锋技能修正
    BossSkillMod shockwave_mod;    // 冲击波修正
    BossSkillMod summon_mod;       // 召唤修正
    const char* evolution_name = nullptr; // "腐化形态" etc.
    bool  arena_effect = false;    // 是否激活Arena变化
};

// 计算Boss Evolution (在boss spawn时调用, 不修改BossAI核心逻辑)
BossEvolutionData calc_boss_evolution(int boss_floor,
    const WorldState& ws, BuildType bt, const class RelationshipSystem& rels);

// Build反制描述 (供HUD使用)
const char* get_build_counter_desc(int boss_floor, BuildType bt);
