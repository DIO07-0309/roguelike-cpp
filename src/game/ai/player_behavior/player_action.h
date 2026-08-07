#pragma once
#include "raylib.h"
#include <cstdint>

// ============================================================
// F15.2: PlayerAction — serializable player event
// Lightweight, no dependency on Player/CombatSystem classes.
// Collected into a timeline that Boss AI can replay.
// ============================================================

enum class PlayerActionType : int {
    NONE = 0,
    ATTACK,       // weapon combo hit
    SKILL,        // active skill used
    MOVE,         // movement state change (idle→move or direction change)
    DODGE,        // large displacement in one tick
    TAKE_DAMAGE,  // player received damage
    DEAL_DAMAGE,  // player dealt damage
    HEAL,         // player used healing
    FLOOR_ENTER,  // entered new floor
};

// 是否为战斗决策动作 (可供行为克隆/在线观察学习)
inline bool is_decision_action(PlayerActionType t) {
    return t == PlayerActionType::ATTACK || t == PlayerActionType::SKILL
        || t == PlayerActionType::DODGE || t == PlayerActionType::HEAL;
}

struct PlayerAction {
    PlayerActionType type = PlayerActionType::NONE;
    float timestamp = 0.0f;   // seconds since game start
    int floor = 0;
    float pos_x = 0.0f, pos_y = 0.0f;
    int value = 0;            // damage dealt/taken, or skill_id, or movement direction
    int skill_id = -1;        // -1=N/A, 0=slash, 1=fireball, 2=self_heal, 3=the_world
    const char* weapon_name = nullptr;  // nullptr unless ATTACK

    // M1: decision context snapshot at the moment of the action.
    // Filled by PlayerBehaviorRecorder::record() from the latest frame context.
    float hp = -1.0f;          // player HP ratio [0,1], -1 = unknown (legacy stream)
    float enemy_dist = -1.0f;  // distance to nearest alive enemy in tiles, -1 = unknown
    int skill_ready_mask = 0;  // bit i = skill i cooldown ready (bit 0 = slash)
    // M5: 条件维度 — 朝向 (Direction 枚举: 0=下 1=上 2=左 3=右, -1=无) + 近 1s 内受击 (受压态势)
    int  facing_dir = -1;
    int  hit_in_1s = 0;        // 0=无受压, >0=刚被打 (理解"被打后行为变化")
    // M4.4: 战术链符号素材 — 武器类型 + 连招段 (仅 ATTACK 有效)
    int  weapon_type = -1;     // WeaponType 枚举 int, -1=未知
    int  combo_stage = -1;     // 0/1/2 连招段, -1=未知
};
