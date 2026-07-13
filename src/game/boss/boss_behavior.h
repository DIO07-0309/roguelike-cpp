#pragma once
#include <string>
#include <vector>
#include "world_state.h"
#include "build_score.h"

class Player;
class Monster;
class GameMap;

// ============================================================
// D5 Step3: Adaptive Boss Behavior — 动态决策 + 人格 + 记忆
// ============================================================

enum class BossDecision {
    CHASE, RETREAT, CHARGE, SUMMON, DEFEND,
    RANGED, MELEE, SPECIAL, PHASE_CHANGE, LAST_STAND, IDLE
};

enum class BossPersonality { AGGRESSIVE, AREA_CONTROL, MANIPULATOR };

struct BossContext {
    float hp_pct;           // Boss当前HP百分比 0-1
    float dist_tiles;       // 到玩家的距离(格)
    bool  player_low_hp;    // 玩家HP<30%
    bool  player_using_skill;
    bool  player_combo_high; // combo≥4
    bool  player_far;        // >7 tiles
    bool  player_near;       // <3 tiles
    BuildType build;
    StoryStage stage;
    bool  last_stand;
    bool  near_arena_danger;  // 玩家附近有Arena危险
};

// 玩家行为记忆 (最近30s)
struct BossMemory {
    int  attacks = 0;
    int  skills = 0;
    int  heals = 0;
    int  dodges = 0;
    int  combos_max = 0;
    int  potions_used = 0;
    float timer = 0; // >30s自动重置

    void record_attack() { attacks++; }
    void record_skill()  { skills++; }
    void record_heal()   { heals++; }
    void record_dodge()  { dodges++; }
    void record_combo(int c) { if (c > combos_max) combos_max = c; }
    void tick(float dt, bool reset);
};

struct BossBehaviorState {
    BossDecision current = BossDecision::IDLE;
    BossDecision last_decision = BossDecision::IDLE;
    float decision_timer = 0;        // 决策冷却
    BossPersonality personality = BossPersonality::AGGRESSIVE;
    BossMemory memory;
    const char* decision_name = nullptr;
    const char* personality_name = nullptr;
    int weights[11] = {0};           // 各Decision权重 (F11 debug)
};

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
