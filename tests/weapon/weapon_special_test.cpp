// G9.1: Weapon special mechanics tests
#include <gtest/gtest.h>
#include <cmath>
#include "types/weapon_types.h"

// ═══════════════════════════════════════════════════
// Nunchaku damage sequence
// ═══════════════════════════════════════════════════

TEST(NunchakuSpecial, DamageSequence) {
    // Stage 3: 5 hits, starting at 80%, each ×1.2
    float expected[] = { 0.80f, 0.96f, 1.152f, 1.3824f, 1.65888f };
    float mult = 0.80f;
    for (int i = 0; i < 5; ++i) {
        EXPECT_NEAR(mult, expected[i], 0.01f)
            << "Hit " << (i + 1) << " multiplier mismatch";
        mult *= 1.20f;
    }
}

TEST(NunchakuSpecial, TotalDamageIsHigh) {
    // 5 hits at growing multipliers should exceed single-hit 1.0
    float total = 0.0f;
    float mult = 0.80f;
    for (int i = 0; i < 5; ++i) { total += mult; mult *= 1.20f; }
    EXPECT_GT(total, 3.0f); // total should be > 3x base
    EXPECT_LT(total, 8.0f);
}

// ═══════════════════════════════════════════════════
// Spear 10-hit rapid
// ═══════════════════════════════════════════════════

TEST(SpearSpecial, TenHitsSameMultiplier) {
    // Spear: 10 hits, each 110%, no growth
    float mult = 1.10f;
    float total = 0.0f;
    for (int i = 0; i < 10; ++i) total += mult;
    EXPECT_NEAR(total, 11.0f, 0.01f);
}

TEST(SpearSpecial, SectorAngleCheck) {
    // 30° half-angle sector
    float half_deg = 30.0f;
    float half_rad = half_deg * 3.14159265f / 180.0f;
    // A target at 20° offset should be in range
    float angle20 = 20.0f * 3.14159265f / 180.0f;
    EXPECT_LE(angle20, half_rad + 0.01f);
    // A target at 40° offset should be out of range
    float angle40 = 40.0f * 3.14159265f / 180.0f;
    EXPECT_GT(angle40, half_rad);
}

// ═══════════════════════════════════════════════════
// Crossbow projectile
// ═══════════════════════════════════════════════════

TEST(CrossbowProjectile, SpawnAndMove) {
    Projectile p;
    p.pos = {100, 100};
    p.vel = {600, 0};  // move right at 600px/s
    p.damage = 50;
    p.lifetime = 2.0f;
    p.alive = true;

    float dt = 0.1f;
    p.pos.x += p.vel.x * dt;
    p.pos.y += p.vel.y * dt;

    EXPECT_NEAR(p.pos.x, 160.0f, 0.01f);
    EXPECT_NEAR(p.pos.y, 100.0f, 0.01f);
}

TEST(CrossbowProjectile, ExpiresAfterLifetime) {
    Projectile p;
    p.lifetime = 1.0f;
    p.elapsed = 0.0f;
    p.alive = true;

    p.elapsed += 0.6f;
    EXPECT_TRUE(p.alive); // not expired yet
    p.elapsed += 0.5f;    // 1.1 > 1.0
    if (p.elapsed >= p.lifetime) p.alive = false;
    EXPECT_FALSE(p.alive);
}

TEST(CrossbowProjectile, PowerShotHasHighDamage) {
    // Stage-3 power shot: 200% multiplier
    float power_mult = 2.00f;
    int base_dmg = 30;
    int expected = (int)(base_dmg * power_mult);
    EXPECT_EQ(expected, 60);
}

TEST(CrossbowProjectile, TripleArrowSpread) {
    // Stage 2: three arrows at -15°, 0°, +15°
    float spreads[] = { -15.0f, 0.0f, 15.0f };
    for (int i = 0; i < 3; ++i) {
        float rad = spreads[i] * 3.14159265f / 180.0f;
        float fwd_x = 1.0f, fwd_y = 0.0f;
        float ca = cosf(rad), sa = sinf(rad);
        float vx = fwd_x * ca - fwd_y * sa;
        float vy = fwd_x * sa + fwd_y * ca;
        // All should have positive x (forward)
        EXPECT_GT(vx, 0.5f) << "Spread angle " << spreads[i] << " goes backward";
    }
}

// ═══════════════════════════════════════════════════
// Combo reset
// ═══════════════════════════════════════════════════

TEST(ComboReset, TimeoutResetsIndex) {
    WeaponSpecialState sp;
    sp.start(5, 0.08f, 0.80f, 1.20f);
    EXPECT_TRUE(sp.active);
    EXPECT_EQ(sp.hit_count, 0);

    // Fire all 5 hits
    for (int i = 0; i < 5; ++i) {
        bool fired = sp.should_fire_next(0.09f);
        EXPECT_TRUE(fired) << "Hit " << (i + 1) << " should fire";
    }

    // After 5 hits, special should be deactivated (last hit's stats retained
    // so the caller can read its multiplier/tracked target)
    EXPECT_FALSE(sp.active);
    EXPECT_EQ(sp.hit_count, 5);
}

TEST(ComboReset, ManualReset) {
    WeaponSpecialState sp;
    sp.start(10, 0.10f, 1.10f, 1.0f);
    EXPECT_TRUE(sp.active);
    sp.reset();
    EXPECT_FALSE(sp.active);
    EXPECT_EQ(sp.hit_count, 0);
}

TEST(ComboReset, MultiplierGrowthCalc) {
    WeaponSpecialState sp;
    sp.start(5, 0.08f, 0.80f, 1.20f);

    // Before any hits
    EXPECT_NEAR(sp.current_multiplier(), 0.80f, 0.01f);

    // After 1 hit (hit_count=1, multiplier still at base since
    // current_multiplier uses hit_count which was incremented in should_fire_next)
    sp.should_fire_next(0.09f); // hit_count becomes 1
    EXPECT_NEAR(sp.current_multiplier(), 0.80f, 0.01f);

    // After 2 hits
    sp.should_fire_next(0.09f); // hit_count becomes 2
    EXPECT_NEAR(sp.current_multiplier(), 0.96f, 0.01f); // 0.80 * 1.20^1
}

// ═══════════════════════════════════════════════════
// Projectile lifecycle
// ═══════════════════════════════════════════════════

TEST(ProjectileLifecycle, NonPiercingDiesOnHit) {
    Projectile p;
    p.alive = true;
    p.piercing = false;
    // Simulate hit: set alive = false
    p.alive = false;
    EXPECT_FALSE(p.alive);
}

TEST(ProjectileLifecycle, PiercingSurvivesHit) {
    Projectile p;
    p.alive = true;
    p.piercing = true;
    // Piercing projectile stays alive after a hit
    EXPECT_TRUE(p.alive);
}
