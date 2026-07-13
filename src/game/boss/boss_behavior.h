#pragma once
#include <string>
#include <vector>
#include "types/boss_types.h"

class Player;
class Monster;
class GameMap;

// D5 Step3: Adaptive Boss Behavior — 动态决策 + 人格 + 记忆

// 计算当前最优决策 (不修改BossAI, 仅返回推荐)
BossDecision evaluate_boss_decision(int boss_floor,
    const BossContext& ctx,
    const WorldState& ws,
    const class RelationshipSystem& rels,
    BossBehaviorState& state,
    float dt);

// 人格初始化
BossPersonality boss_personality_for_floor(int floor);
const char* decision_name(BossDecision d);
const char* personality_name(BossPersonality p);
int  decision_weight(BossDecision d, const BossContext& ctx,
                     BossPersonality pers, const BossMemory& mem,
                     const WorldState& ws, int boss_floor);
