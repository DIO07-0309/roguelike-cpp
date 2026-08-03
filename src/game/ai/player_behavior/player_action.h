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

struct PlayerAction {
    PlayerActionType type = PlayerActionType::NONE;
    float timestamp = 0.0f;   // seconds since game start
    int floor = 0;
    float pos_x = 0.0f, pos_y = 0.0f;
    int value = 0;            // damage dealt/taken, or skill_id, or movement direction
    int skill_id = -1;        // -1=N/A, 0=slash, 1=fireball, 2=self_heal, 3=the_world
    const char* weapon_name = nullptr;  // nullptr unless ATTACK
};
