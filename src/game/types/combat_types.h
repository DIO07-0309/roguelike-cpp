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

// ── G10.2: DamageResult (type-aware, consistent pipeline) ──
struct DamageResult {
    int raw_damage = 0;
    int final_damage = 0;
    int damage_type = 0;  // AttackType as int (PHYSICAL=0, MAGICAL=1, TRUE=2)
    bool critical = false;
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
    float start_delay = 0;  // G5.8.8: recipe 分镜延迟(秒), 渲染前等待
};

// ── Relic ── (intentionally kept in combat_system.h — field types differ from shared types)
