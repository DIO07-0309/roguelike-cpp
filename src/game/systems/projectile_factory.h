#pragma once
#include "types/weapon_types.h"
#include "entities/combat_stats.h"  // AttackType
#include "components/element_component.h"  // ElementType

class Player;
class Monster;

// ============================================================
// D2: ProjectileFactory — stateless projectile creation
// Single responsibility: assemble a Projectile with correct
// owner, damage, element, and warning phase.
// No Manager. Pure functions. Called from WeaponExecutor + MonsterAI.
// ============================================================

class ProjectileFactory {
public:
    // Create a player-to-monster projectile (crossbow bolt)
    static Projectile player_bolt(Vector2 origin, Vector2 vel,
        int damage, float lifetime = 1.2f, bool piercing = false);

    // Create a monster-to-player warning projectile (archer arrow, mage fireball)
    static Projectile enemy_projectile(Monster* self, Player* target,
        int damage, float speed, float warning_time, WarningLevel level);

    // Create a fixed-position warning AOE (boss ground smash, trap)
    static Projectile warning_aoe(Vector2 pos, float radius,
        int damage, AttackType dtype, float warning_time);

    // Create a spread fan of projectiles from monster toward player
    static std::vector<Projectile> spread_shot(Monster* boss, Player* target,
        int count, float spread_deg, int damage, float speed,
        float warning_time, WarningLevel level);
};
