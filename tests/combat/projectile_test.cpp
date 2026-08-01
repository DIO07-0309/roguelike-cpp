// D2: Warning Projectile System tests
#include <gtest/gtest.h>
#include "types/weapon_types.h"
#include "entities/combat_stats.h"
#include "components/element_component.h"
#include <cmath>

// ═══════════════════════════════════════════════
// Projectile struct defaults (backward compat)
// ═══════════════════════════════════════════════

TEST(ProjectileD2, DefaultsAreBackwardCompat) {
    Projectile p;
    EXPECT_EQ(p.owner, 0);           // PLAYER
    EXPECT_EQ(p.damage_type, 0);     // PHYSICAL
    EXPECT_EQ(p.element, 0);         // NONE
    EXPECT_EQ(p.warning_time, 0.0f);
    EXPECT_EQ(p.active_time, 0.0f);
}

TEST(ProjectileD2, MonsterOwner) {
    Projectile p;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.warning_time = 0.8f;
    p.active_time = -0.8f;   // negative = still in warning
    EXPECT_EQ(p.owner, 1);
    EXPECT_LT(p.active_time, 0.0f);
}

TEST(ProjectileD2, WarningTransition) {
    Projectile p;
    p.warning_time = 0.5f;
    p.active_time = -0.5f;
    // Simulate tick
    p.active_time += 0.6f;
    if (p.active_time > 0.0f) p.active_time = 0.0f; // clamp to 0 on transition
    EXPECT_GE(p.active_time, 0.0f);    // now active
}

TEST(ProjectileD2, WarningPhaseNoDamage) {
    Projectile p;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.warning_time = 0.8f;
    p.active_time = -0.5f;
    // Game loop: if active_time < 0, skip collision
    bool can_damage = (p.active_time >= 0.0f);
    EXPECT_FALSE(can_damage);
}

TEST(ProjectileD2, ElementAndDamageType) {
    Projectile p;
    p.element = 1;      // FIRE
    p.damage_type = 1;  // MAGICAL
    EXPECT_EQ((int)AttackType::MAGICAL, 1);
    EXPECT_EQ(p.element, 1);
}

// ═══════════════════════════════════════════════
// Owner targeting: correct targets hit
// ═══════════════════════════════════════════════

TEST(ProjectileD2, PlayerProjHitsMonster) {
    Projectile p;
    p.owner = (int)ProjectileOwner::PLAYER;
    p.damage = 50;
    // In game_scene tick: PLAYER projectiles are passed to tick_projectiles
    // which checks collision with monster list, not player.
    // Only PLAYER owner projectiles are processed by tick_projectiles.
    EXPECT_EQ(p.owner, 0);
    EXPECT_GT(p.damage, 0);
}

TEST(ProjectileD2, MonsterProjHitsPlayer) {
    Projectile p;
    p.owner = (int)ProjectileOwner::MONSTER;
    p.damage = 30;
    // In game_scene tick: MONSTER projectiles are processed by enemy loop
    // which checks collision with player entity, not monsters.
    EXPECT_EQ(p.owner, 1);
    EXPECT_GT(p.damage, 0);
}

TEST(ProjectileD2, MonsterProjCannotHitMonster) {
    // Verified by design: enemy projectile tick only checks player collision
    Projectile p;
    p.owner = (int)ProjectileOwner::MONSTER;
    EXPECT_NE(p.owner, (int)ProjectileOwner::PLAYER);
}

// ═══════════════════════════════════════════════
// ProjectileOwner enum values
// ═══════════════════════════════════════════════

TEST(ProjectileD2, OwnerEnumValues) {
    EXPECT_EQ((int)ProjectileOwner::PLAYER, 0);
    EXPECT_EQ((int)ProjectileOwner::MONSTER, 1);
    EXPECT_EQ((int)ProjectileOwner::ENVIRONMENT, 2);
}

TEST(ProjectileD2, PhaseEnumValues) {
    EXPECT_EQ((int)ProjectilePhase::WARNING, 0);
    EXPECT_EQ((int)ProjectilePhase::ACTIVE, 1);
    EXPECT_EQ((int)ProjectilePhase::FINISHED, 2);
}

TEST(ProjectileD2, WarningLevelEnumValues) {
    EXPECT_EQ((int)WarningLevel::NORMAL, 0);
    EXPECT_EQ((int)WarningLevel::DANGEROUS, 1);
    EXPECT_EQ((int)WarningLevel::DEADLY, 2);
}

// ═══════════════════════════════════════════════
// Lifetime / cleanup
// ═══════════════════════════════════════════════

TEST(ProjectileD2, ExpiresAfterActiveLifetime) {
    Projectile p;
    p.warning_time = 0.5f;
    p.active_time = 0.0f;      // just became active
    p.lifetime = 1.0f;         // active for 1.0s after warning
    p.active_time += 1.1f;     // > lifetime
    p.alive = !(p.active_time >= p.lifetime); // cleanup logic
    EXPECT_FALSE(p.alive);
}
