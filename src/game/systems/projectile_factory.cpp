#include "systems/projectile_factory.h"
#include "entities/monster.h"
#include "entities/player.h"
#include "systems/combat_system.h"  // calculate_damage, get_effective_attack
#include <cmath>

// ── Player bolt (existing crossbow behavior) ──
Projectile ProjectileFactory::player_bolt(Vector2 origin, Vector2 vel,
    int damage, float lifetime, bool piercing) {
    Projectile p;
    p.pos = origin;
    p.vel = vel;
    p.damage = damage;
    p.lifetime = lifetime;
    p.piercing = piercing;
    p.alive = true;
    p.owner = (int)ProjectileOwner::PLAYER;
    // element/damage_type default to 0 (NONE/PHYSICAL) — caller can override
    return p;
}

// ── Enemy projectile with warning phase ──
Projectile ProjectileFactory::enemy_projectile(Monster* self, Player* target,
    int damage, float speed, float warning_time, WarningLevel level) {
    float cx = self->entity.rect.x + self->entity.rect.width / 2;
    float cy = self->entity.rect.y + self->entity.rect.height / 2;
    float tx = target->entity.rect.x + target->entity.rect.width / 2;
    float ty = target->entity.rect.y + target->entity.rect.height / 2;
    float dx = tx - cx, dy = ty - cy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) len = 1.0f;

    Projectile p;
    p.pos = {cx, cy};
    p.vel = {dx / len * speed, dy / len * speed};
    p.damage = damage;
    p.lifetime = 3.0f;
    p.alive = true;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.damage_type = (int)self->attack_type;
    // Warning phase
    p.warning_time = warning_time;
    p.active_time = -warning_time; // negative during warning, becomes 0+ when active
    p.warning_level = (int)level;
    p.warning_radius = 24.0f;
    return p;
}

// ── Warning AOE (position-based) ──
Projectile ProjectileFactory::warning_aoe(Vector2 pos, float radius,
    int damage, AttackType dtype, float warning_time) {
    Projectile p;
    p.pos = pos;
    p.damage = damage;
    p.lifetime = warning_time + 0.3f; // brief active window
    p.alive = true;
    p.owner = (int)ProjectileOwner::ENVIRONMENT;
    p.damage_type = (int)dtype;
    p.warning_time = warning_time;
    p.active_time = -warning_time;
    p.warning_level = (int)WarningLevel::DANGEROUS;
    p.warning_radius = radius;
    return p;
}

// ── Spread shot (boss multi-projectile) ──
std::vector<Projectile> ProjectileFactory::spread_shot(Monster* boss, Player* target,
    int count, float spread_deg, int damage, float speed,
    float warning_time, WarningLevel level) {
    std::vector<Projectile> result;
    if (count <= 0) return result;

    float cx = boss->entity.rect.x + boss->entity.rect.width / 2;
    float cy = boss->entity.rect.y + boss->entity.rect.height / 2;
    float tx = target->entity.rect.x + target->entity.rect.width / 2;
    float ty = target->entity.rect.y + target->entity.rect.height / 2;
    float base_angle = atan2f(ty - cy, tx - cx);
    float half = (spread_deg * 3.14159f / 180.0f) / 2.0f;
    float step = (count > 1) ? (spread_deg * 3.14159f / 180.0f) / (float)(count - 1) : 0.0f;

    for (int i = 0; i < count; i++) {
        float angle = base_angle - half + step * (float)i;
        Projectile p;
        p.pos = {cx, cy};
        p.vel = {cosf(angle) * speed, sinf(angle) * speed};
        p.damage = damage;
        p.lifetime = 3.5f;
        p.alive = true;
        p.owner = (int)ProjectileOwner::MONSTER;
        p.damage_type = (int)boss->attack_type;
        p.warning_time = warning_time;
        p.active_time = -warning_time;
        p.warning_level = (int)level;
        p.warning_radius = 20.0f;
        result.push_back(p);
    }
    return result;
}
