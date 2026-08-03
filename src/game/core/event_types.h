#pragma once
#include <string>

// ============================================================
// D7 Step5: 全局事件类型 — 所有 EventBus 事件统一管理
// ============================================================

enum class GameEventType {
    NONE = 0,

    // ── 玩家 ──
    PLAYER_ATTACK,
    PLAYER_DAMAGED,
    PLAYER_DEAD,
    PLAYER_LEVEL_UP,

    // ── 怪物 ──
    MONSTER_SPAWN,
    MONSTER_DIED,

    // ── Boss ──
    BOSS_SPAWN,
    BOSS_PHASE2,
    BOSS_LAST_STAND,
    BOSS_DEAD,

    // ── 物品 / Relic ──
    ITEM_PICKUP,
    ITEM_USE,
    RELIC_GAIN,

    // ── G1: Attack Evolution ──
    ATTACK_EVOLVED,

    // ── G1: Skill Evolution ──
    SKILL_EVOLVED,

    // ── G1: Rule Chain ──
    BOSS_RULE_ACTIVATED,

    // ── Buff ──
    BUFF_APPLIED,
    BUFF_EXPIRED,

    // ── 特殊房间 ──
    SPECIAL_ROOM_ENTER,
    SPECIAL_ROOM_TRIGGER,

    // ── NPC / 对话 ──
    NPC_DIALOGUE_START,
    NPC_DIALOGUE_END,

    // ── 任务 ──
    QUEST_ACCEPT,
    QUEST_COMPLETE,

    // ── 楼层 ──
    FLOOR_ENTER,
    FLOOR_CLEAR,

    // ── G9: Weapon combo events ──
    WEAPON_STAGE_1,
    WEAPON_STAGE_2,
    WEAPON_SPECIAL,

    // ── G9.3: Weapon-skill synergy ──
    WEAPON_ATTACK_COMPLETE,

    // ── G10.3: Element VFX ──
    ELEMENT_FIRE_HIT,
    ELEMENT_FIRE_CRITICAL,
    ELEMENT_ICE_SLOW,
    ELEMENT_ICE_FREEZE,
    ELEMENT_POISON_APPLY,
    ELEMENT_POISON_TICK,
    ELEMENT_LEVEL_UP,

    // ── F10.1: Boss arena domain events ──
    BOSS_DOMAIN_ENTER,
    BOSS_VULNERABLE_ENTER,
    WEAK_POINT_BREAK,

    // ── 游戏 ──
    GAME_CLEAR,
    GAME_OVER,
    ENDING_REACHED,
    META_GAIN,

    COUNT
};

// 轻量事件数据体
struct GameEvent {
    GameEventType type = GameEventType::NONE;
    void* sender = nullptr;
    int   int_val = 0;           // floor / damage / level / rarity
    float float_val = 0.0f;      // timer / pct
    const char* str_val = nullptr;   // name / id / msg
};
