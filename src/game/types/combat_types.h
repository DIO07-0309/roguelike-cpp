#pragma once
// ============================================================
// D7 Step3: combat_types.h — 战斗系统共享数据结构
// ============================================================
#include <string>
#include <vector>
#include "entity.h"
#include "build_tag.h"

// ── Buff 系统 ──
struct BuffDef {
    std::string id;
    float duration = 0.0f;
    int max_stacks = 1;
    float tick_interval = 0.0f;
    int tick_damage = 0;
    std::string display_name, short_name;
    unsigned char hud_color_r = 200, hud_color_g = 200, hud_color_b = 200;
    std::vector<BuildTag> tags;
};

enum class BuffEventType { APPLIED, TICK_DAMAGE, EXPIRED };
struct BuffEvent {
    BuffEventType type;
    std::string buff_id, target;
    int stacks = 0, value = 0;
};

enum class BuffTarget { SELF, ENEMY };
struct BuffTrigger {
    std::string buff_id;
    int stacks = 1;
    float chance = 1.0f;
    BuffTarget target = BuffTarget::ENEMY;
};

// ── VFX ──
struct Effect {
    std::string kind;
    float world_x, world_y;
    float radius = 32;
    Color color{255, 200, 50, 255};
    float duration = 0.35f, elapsed = 0.0f;
    Direction direction = Direction::DOWN;
    float target_x = 0, target_y = 0;
    int level = 1;
};

// ── Relic ── (intentionally kept in combat_system.h — field types differ from shared types)
