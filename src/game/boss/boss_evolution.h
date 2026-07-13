#pragma once
#include "types/boss_types.h"

// D5 Step2: Boss Evolution — Adaptive Boss AI & Skill Variants

// 计算Boss Evolution (在boss spawn时调用)
BossEvolutionData calc_boss_evolution(int boss_floor,
    const WorldState& ws, BuildType bt, const class RelationshipSystem& rels);
const char* get_build_counter_desc(int boss_floor, BuildType bt);
