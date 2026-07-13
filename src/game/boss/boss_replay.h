#pragma once
#include <string>
#include "build_score.h"

// ============================================================
// D5 Step5: Boss Replay — Boss学习与战斗评估
// ============================================================

enum class BossRank { D, C, B, A, S, SS };

struct BossReplayMemory {
    int melee_hits = 0, ranged_hits = 0, skills_used = 0;
    int dodge_count = 0, potion_used = 0, combo_max = 0;
    float survive_time = 0.0f;
    BuildType build = BuildType::NONE;
    bool blood_ritual = false, curse = false, saved_priest = false;

    // 每20秒分析, 返回适应策略(0-5)
    int analyze_strategy() const;
    const char* strategy_name() const;
};

struct BossBattleReport {
    BossReplayMemory replay;
    BossRank rank = BossRank::C;
    int total_damage = 0, damage_taken = 0;
    float battle_time = 0.0f;
    int arena_zones_spawned = 0;
    const char* rank_name() const;
};

BossRank calc_boss_rank(const BossReplayMemory& mem, int damage_taken, int damage_done, float time);
BossBattleReport generate_battle_report(const BossReplayMemory& mem, int dmg_done, int dmg_taken, float time, int arena_zones);
