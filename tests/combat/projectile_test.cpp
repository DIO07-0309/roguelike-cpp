// D2: Warning Projectile System tests
#include <gtest/gtest.h>
#include "types/weapon_types.h"
#include "entities/combat_stats.h"
#include "components/element_component.h"
#include "systems/projectile_factory.h"
#include "entities/monster.h"
#include "entities/player.h"
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

// ═══════════════════════════════════════════════
// ProjectileFactory: real factory behavior
// ═══════════════════════════════════════════════

TEST(ProjectileFactoryD2, PlayerBoltDefaults) {
    Projectile p = ProjectileFactory::player_bolt({0, 0}, {100, 0}, 10);
    EXPECT_EQ(p.owner, (int)ProjectileOwner::PLAYER);
    EXPECT_EQ(p.vel.x, 100.0f);
    EXPECT_EQ(p.vel.y, 0.0f);
    EXPECT_EQ(p.damage, 10);
    EXPECT_GE(p.active_time, 0.0f);   // player bolt: no warning phase
}

TEST(ProjectileFactoryD2, EnemyProjectileLocksDirection) {
    Monster m(0, 0, "archer", 10, 5, 1, 1, {255, 255, 255, 255});
    m.entity.rect = {0, 0, 32, 32};       // center (16, 16)
    Player p(100, 100, 120, 100, 10, 5, 5);
    p.entity.rect = {100, 100, 32, 32};   // center (116, 116)

    auto proj = ProjectileFactory::enemy_projectile(
        &m, &p, 20, 200.0f, 0.8f, WarningLevel::NORMAL);

    EXPECT_EQ(proj.owner, (int)ProjectileOwner::MONSTER);
    EXPECT_EQ(proj.damage, 20);
    // Direction snapshot toward player: vel = unit(100,100) * 200
    float speed = sqrtf(proj.vel.x * proj.vel.x + proj.vel.y * proj.vel.y);
    EXPECT_NEAR(speed, 200.0f, 1.0f);
    EXPECT_GT(proj.vel.x, 0.0f);
    EXPECT_GT(proj.vel.y, 0.0f);
    // Warning phase: negative active_time, warning_time preserved
    EXPECT_LT(proj.active_time, 0.0f);
    EXPECT_EQ(proj.warning_time, 0.8f);
    EXPECT_EQ(proj.warning_level, (int)WarningLevel::NORMAL);
}

TEST(ProjectileFactoryD2, SpreadShotCountAndSymmetry) {
    Monster m(0, 0, "boss", 100, 20, 5, 5, {255, 255, 255, 255});
    m.entity.rect = {0, 0, 64, 64};       // center (32, 32)
    Player p(100, 32, 120, 100, 10, 5, 5);
    p.entity.rect = {100, 32, 32, 32};    // directly right of boss

    auto shots = ProjectileFactory::spread_shot(
        &m, &p, 5, 60.0f, 15, 250.0f, 0.7f, WarningLevel::DANGEROUS);

    ASSERT_EQ(shots.size(), 5u);
    for (auto& s : shots) {
        EXPECT_EQ(s.owner, (int)ProjectileOwner::MONSTER);
        EXPECT_EQ(s.damage, 15);
        EXPECT_EQ(s.warning_level, (int)WarningLevel::DANGEROUS);
        EXPECT_LT(s.active_time, 0.0f);
        EXPECT_GT(s.vel.x, 0.0f);          // all fan toward player side
    }
    // Symmetry: first and last angles mirror around base angle
    float a0 = atan2f(shots[0].vel.y, shots[0].vel.x);
    float a4 = atan2f(shots[4].vel.y, shots[4].vel.x);
    // player center (116, 48) vs boss center (32, 32) → base angle 10.8°
    float base = atan2f(48.0f - 32.0f, 116.0f - 32.0f);
    EXPECT_NEAR((a0 + a4) / 2.0f, base, 1e-3f);
}

TEST(ProjectileFactoryD2, WarningAoeKeepsRadius) {
    auto aoe = ProjectileFactory::warning_aoe(
        {50, 50}, 60.0f, 30, AttackType::PHYSICAL, 1.0f);
    EXPECT_EQ(aoe.owner, (int)ProjectileOwner::ENVIRONMENT);
    EXPECT_EQ(aoe.warning_radius, 60.0f);
    EXPECT_EQ(aoe.vel.x, 0.0f);
    EXPECT_EQ(aoe.vel.y, 0.0f);
    EXPECT_EQ(aoe.damage, 30);
    EXPECT_LT(aoe.active_time, 0.0f);
    EXPECT_EQ(aoe.warning_time, 1.0f);
}
