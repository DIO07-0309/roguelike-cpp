#pragma once
// ============================================================
// G9: weapon_types.h — weapon system shared data structures
// ============================================================
#include <string>
#include <vector>
#include <cstdint>
#include "raylib.h"
#include "entity.h"  // Direction enum

class Monster;  // forward decl for WeaponAttackResult

// ── Weapon classification ──
enum class WeaponType : int {
    FIST = 0,
    DAGGER,
    SWORD,
    NUNCHAKU,
    CROSSBOW,
    SPEAR,
    COUNT
};

inline const char* weapon_type_name(WeaponType wt) {
    switch (wt) {
    case WeaponType::FIST:     return "FIST";
    case WeaponType::DAGGER:   return "DAGGER";
    case WeaponType::SWORD:    return "SWORD";
    case WeaponType::NUNCHAKU: return "NUNCHAKU";
    case WeaponType::CROSSBOW: return "CROSSBOW";
    case WeaponType::SPEAR:    return "SPEAR";
    default: return "UNKNOWN";
    }
}

inline WeaponType weapon_type_from_string(const char* s) {
    if (!s) return WeaponType::FIST;
    std::string t(s);
    if (t == "DAGGER")   return WeaponType::DAGGER;
    if (t == "SWORD")    return WeaponType::SWORD;
    if (t == "NUNCHAKU") return WeaponType::NUNCHAKU;
    if (t == "CROSSBOW") return WeaponType::CROSSBOW;
    if (t == "SPEAR")    return WeaponType::SPEAR;
    return WeaponType::FIST;
}

// ── G9.4: Weapon sprite registry key — 素材未覆盖的类型返回空 ──
inline const char* weapon_sprite_key(WeaponType wt) {
    switch (wt) {
        case WeaponType::DAGGER:   return "weapon_dagger";
        case WeaponType::SWORD:    return "weapon_sword";
        case WeaponType::SPEAR:    return "weapon_spear";
        case WeaponType::NUNCHAKU: return "weapon_nunchaku";
        case WeaponType::CROSSBOW: return "weapon_crossbow";
        default:                   return nullptr;
    }
}

// ── Hit shape for attack detection ──
enum class HitShape : int {
    CIRCLE = 0,
    SECTOR,
    RECTANGLE,
    CAPSULE,
    PROJECTILE
};

inline HitShape hit_shape_from_string(const char* s) {
    if (!s) return HitShape::CIRCLE;
    std::string t(s);
    if (t == "SECTOR")     return HitShape::SECTOR;
    if (t == "RECTANGLE")  return HitShape::RECTANGLE;
    if (t == "CAPSULE")    return HitShape::CAPSULE;
    if (t == "PROJECTILE") return HitShape::PROJECTILE;
    return HitShape::CIRCLE;
}

// ── Per-stage attack definition (data-driven) ──
struct AttackStageDef {
    float damage_multiplier = 1.0f;
    HitShape hit_shape = HitShape::CIRCLE;
    int damage_type = 0;          // G10.2: AttackType (0=PHYSICAL, 1=MAGICAL, 2=TRUE)
    float range = 1.0f;          // in tiles (range * TILE_SIZE = pixels)
    float width = 0.5f;          // secondary dimension (sector angle/rect width/capsule radius)
    float recovery = 0.2f;       // after-attack delay (seconds)
    float windup = 0.05f;        // pre-attack delay (seconds)
    float hit_frame = 0.1f;      // time into animation when damage applies
    float cancel_window = 0.6f;  // fraction of recovery that is cancelable
    std::string vfx_recipe;      // VFX recipe id string
    float camera_shake = 2.0f;   // shake intensity
    std::string sfx_name;        // sound effect id
};

// ── G9.2: Weapon affix — per-stage bonus effect ──
struct AffixDef {
    std::string type;    // "bleed" | "range_boost" | "damage_ramp" | "cd_reduce" | "pierce_bonus" | ""
    float value = 0.0f;  // magnitude (10 = +10% bleed chance, 0.5 = +50% range, etc.)
};

// ── Weapon definition (data-driven, read-only) ──
struct WeaponDef {
    std::string id;              // "dagger_common", "sword_legendary", etc.
    std::string name;            // display name (base / common name)
    WeaponType type = WeaponType::FIST;
    std::string rarity;          // "common"|"rare"|"epic"|"legendary"
    float base_damage = 5.0f;
    float base_range = 1.0f;
    float min_range = 0.0f;
    float max_range = 0.0f;
    float combo_timeout = 0.80f;
    AttackStageDef stages[3];
    int stage_count = 1;
    Color quality_colors[4];     // per-rarity tint colors

    // ── G9.2: Naming ──
    std::string rare_names[2];     // 2 random-roll names for rare tier
    std::string epic_names[2];     // 2 random-roll names for epic tier
    std::string legendary_name;    // fixed name for legendary tier

    // ── G9.2: Affix (per-stage, applied in WeaponExecutor) ──
    AffixDef affix;                // weapon-type-specific bonus

    // ── G9.2: Legendary effect ──
    std::string legendary_effect;  // "" | "sword_wave" | "dagger_bleed" | "nunchaku_hits" | "crossbow_power" | "spear_count"
};

// ── G9.2: Pick a random display name based on rarity tier ──
inline const char* pick_weapon_name(const WeaponDef* def, int rarity_tier) {
    if (!def) return "";
    // rarity_tier: 0=common, 1=rare, 2=epic, 3=legendary
    if (rarity_tier >= 3 && !def->legendary_name.empty())
        return def->legendary_name.c_str();
    if (rarity_tier == 2) {
        const auto& pool = def->epic_names;
        // Simple hash-based pick (deterministic per run, not uniform — good enough)
        int pick = pool[0].empty() ? 0 : ((int)(def->base_damage * 100 + def->base_range * 10) % 2);
        return pool[pick].empty() ? def->name.c_str() : pool[pick].c_str();
    }
    if (rarity_tier == 1) {
        const auto& pool = def->rare_names;
        int pick = pool[0].empty() ? 0 : ((int)(def->base_damage * 73 + def->base_range * 37) % 2);
        return pool[pick].empty() ? def->name.c_str() : pool[pick].c_str();
    }
    return def->name.c_str(); // common: base name
}

// ── D2: Projectile owner/phase/warning enums ──
enum class ProjectileOwner : int { PLAYER = 0, MONSTER, ENVIRONMENT };
enum class ProjectilePhase : int { WARNING = 0, ACTIVE, FINISHED };
enum class WarningLevel : int { NORMAL = 0, DANGEROUS, DEADLY };

// ── G9.1/D2: Unified Projectile (crossbow bolts + enemy attacks + traps) ──
struct Projectile {
    Vector2 pos{};
    Vector2 vel{};          // direction * speed, pre-computed
    int damage = 0;
    float lifetime = 2.0f;
    float elapsed = 0.0f;
    bool piercing = false;   // passes through entities (NOT walls)
    bool pierce_walls = false; // passes through walls (boss wind-up, crossbow power shot)
    bool alive = true;

    // D2: Owner, element, damage type
    int owner = 0;           // ProjectileOwner as int (0=PLAYER)
    int element = 0;         // ElementType as int (0=NONE)
    int damage_type = 0;     // AttackType as int (0=PHYSICAL)

    // D2: Warning phase — AI-readable timing
    float warning_time = 0.0f;   // seconds in WARNING phase
    float active_time = 0.0f;    // seconds since becoming ACTIVE (0 until WARNING ends)
    int warning_level = 0;       // WarningLevel as int (0=NORMAL)
    float warning_radius = 0.0f; // AOE warning circle radius (0 = point projectile)
};

// ── G9.1: Multi-hit special state (nunchaku flurry, spear rapid) ──
struct WeaponSpecialState {
    bool active = false;
    float timer = 0.0f;
    float next_hit_at = 0.0f;   // timer threshold for next auto-hit
    float hit_interval = 0.1f;
    int hit_count = 0;
    int max_hits = 0;
    float base_mult = 1.0f;
    float mult_growth = 1.0f;   // ×1.2 for nunchaku, ×1.0 for spear
    uint64_t tracked_instance = 0;  // nunchaku auto-track target (Monster::instance_id, 指针跨层残留会进程间分叉)
    float range_px = 0.0f;
    float width_param = 0.0f;
    int hit_shape = 0;          // stored HitShape as int
    Direction direction = Direction::DOWN;

    void start(int max_h, float interval, float mult, float growth);
    bool should_fire_next(float dt); // advance timer, return true if hit should fire
    float current_multiplier() const;
    void reset();
};

// ═══════════════════════════════════════════════════════════════
// G10.5-B: AttackGeometry — 单段攻击的唯一空间真相 (SSOT)
// hit_detection 与 VFX 必须消费同一份几何, 禁止两侧各自估像素
//   origin/direction: executor 计算的玩家碰撞中心与朝向
//   shape/range/width: weapons.json 每段定义 (px 换算后)
//   视觉允许 shape 忠实呈现 + 少量表现边缘 (如 x1.1 弧外沿)
// ═══════════════════════════════════════════════════════════════
struct AttackGeometry {
    Vector2 origin{};      // 攻击原点 (= 玩家碰撞盒中心, px)
    Vector2 direction{};   // 单位朝向向量
    HitShape shape = HitShape::CIRCLE;
    float range_px = 0.0f;  // 主尺寸: 半径/前伸长度 (px)
    float width_px = 0.0f;  // 副尺寸: 扇形半角(deg) / 矩形半宽 / 胶囊半径 (px/deg)
};

// ── Shared result type for weapon attack execution ──
struct WeaponAttackResult {
    Monster* target = nullptr;
    int damage = 0;
    bool is_crit = false;
    bool is_killing_blow = false;
    Vector2 hit_point{};
    bool from_special = false;  // G9.1: true if from tick-based multi-hit
    AttackGeometry geometry{};   // G10.5-B: 本段判定几何 — VFX 消费此真相
};

// ═══════════════════════════════════════════════════════════════
// G9.3: AttackTag + AttackContext — weapon→skill bridge
// ═══════════════════════════════════════════════════════════════

enum class AttackTag : int {
    NONE = 0,
    SLASH,         // dagger/sword slash stages
    PIERCE,        // spear/dagger thrust
    BLUNT,         // sword ground smash
    RANGED,        // crossbow bolts
    MULTI_HIT,     // nunchaku flurry, spear rapid
    KNOCKBACK,     // nunchaku push-back
    MARKED,        // crossbow power shot
    PIERCE_STACK   // spear pierce chain
};

inline const char* attack_tag_name(AttackTag t) {
    switch (t) {
    case AttackTag::SLASH:       return "SLASH";
    case AttackTag::PIERCE:      return "PIERCE";
    case AttackTag::BLUNT:       return "BLUNT";
    case AttackTag::RANGED:      return "RANGED";
    case AttackTag::MULTI_HIT:   return "MULTI_HIT";
    case AttackTag::KNOCKBACK:   return "KNOCKBACK";
    case AttackTag::MARKED:      return "MARKED";
    case AttackTag::PIERCE_STACK: return "PIERCE_STACK";
    default: return "NONE";
    }
}

// Lightweight context written by WeaponExecutor, read by Skill::execute()
struct AttackContext {
    WeaponType weapon_type = WeaponType::FIST;
    AttackTag primary_tag = AttackTag::NONE;
    int combo_stage = 0;
    bool stage3_triggered = false;
    float damage_dealt = 0.0f;
    Monster* last_target = nullptr;
    float timestamp = 0.0f;

    void reset() { *this = AttackContext{}; }
    bool valid(float t) const {
        return t >= timestamp && (t - timestamp) < 2.0f && primary_tag != AttackTag::NONE;
    }
};
