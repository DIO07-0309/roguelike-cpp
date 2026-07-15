#pragma once
#include "types/boss_types.h"

// D5 Step5: Boss Replay — Boss学习与战斗评估
// D8 Step2: extended stats (summon_count, shield_time, phase_time)

BossRank calc_boss_rank(const BossReplayMemory& mem, int damage_taken, int damage_done, float time);
BossBattleReport generate_battle_report(const BossReplayMemory& mem, int dmg_done, int dmg_taken,
                                        float time, int arena_zones);
