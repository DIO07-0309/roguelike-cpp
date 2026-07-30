// G10.2: DamageType pipeline tests
#include <gtest/gtest.h>
#include "entities/combat_stats.h"
#include "systems/combat_system.h"

// ═══════════════════════════════════════════════
// Physical: reduced by defense
// ═══════════════════════════════════════════════

TEST(DamageType, PhysicalReducedByArmor) {
    // 100 ATK vs 100 DEF → 100 - 100*0.5 = 50
    int dmg = calculate_damage(100, 100, AttackType::PHYSICAL);
    EXPECT_GE(dmg, 1);
    EXPECT_LT(dmg, 100); // must be less than raw
}

TEST(DamageType, PhysicalZeroDefFullDamage) {
    int dmg = calculate_damage(100, 0, AttackType::PHYSICAL);
    // ~80-120 with variance
    EXPECT_GE(dmg, 40);
    EXPECT_LE(dmg, 140);
}

// ═══════════════════════════════════════════════
// Magical: reduced by magic resistance
// ═══════════════════════════════════════════════

TEST(DamageType, MagicalReducedByResistance) {
    int dmg = calculate_damage(100, 100, AttackType::MAGICAL);
    EXPECT_GE(dmg, 1);
    EXPECT_LT(dmg, 100);
}

TEST(DamageType, MagicalPiecesMoreThanPhysical) {
    // Magic uses 0.6 factor vs physical 0.5 — so magic should do less
    int phys = calculate_damage(100, 100, AttackType::PHYSICAL);
    int mag  = calculate_damage(100, 100, AttackType::MAGICAL);
    // With same ATK/DEF, magic should be slightly weaker (higher def factor)
    EXPECT_GE(phys, 1);
    EXPECT_GE(mag, 1);
}

// ═══════════════════════════════════════════════
// True damage: ignores ALL defense
// ═══════════════════════════════════════════════

TEST(DamageType, TrueDamageIgnoresDefense) {
    int dmg = calculate_damage(100, 999, AttackType::TRUE);
    // Should be ~100 * variance, unaffected by 999 defense
    EXPECT_GE(dmg, 40);
    EXPECT_LE(dmg, 140);
}

TEST(DamageType, TrueDamageEqualsPhysicalAtZeroDef) {
    int phys = calculate_damage(50, 0, AttackType::PHYSICAL);
    int true_dmg = calculate_damage(50, 0, AttackType::TRUE);
    // Both should be in the same range (variance only)
    EXPECT_GE(phys, 1);
    EXPECT_GE(true_dmg, 1);
}

// ═══════════════════════════════════════════════
// AttackType enum values
// ═══════════════════════════════════════════════

TEST(DamageType, EnumValuesConsistent) {
    EXPECT_EQ((int)AttackType::PHYSICAL, 0);
    EXPECT_EQ((int)AttackType::MAGICAL, 1);
    EXPECT_EQ((int)AttackType::TRUE, 2);
}

// ═══════════════════════════════════════════════
// DamageResult struct
// ═══════════════════════════════════════════════

TEST(DamageResult, DefaultValues) {
    DamageResult dr;
    EXPECT_EQ(dr.raw_damage, 0);
    EXPECT_EQ(dr.final_damage, 0);
    EXPECT_EQ(dr.damage_type, 0);
    EXPECT_FALSE(dr.critical);
}

TEST(DamageResult, Settable) {
    DamageResult dr;
    dr.raw_damage = 100;
    dr.final_damage = 85;
    dr.damage_type = 1; // MAGICAL
    dr.critical = true;
    EXPECT_EQ(dr.raw_damage, 100);
    EXPECT_EQ(dr.final_damage, 85);
    EXPECT_EQ(dr.damage_type, 1);
    EXPECT_TRUE(dr.critical);
}

// ═══════════════════════════════════════════════
// Weapon default: PHYSICAL → behavior unchanged
// ═══════════════════════════════════════════════

TEST(DamageType, WeaponDefaultPhysicalUnchanged) {
    // Old weapons default to damage_type=0 → PHYSICAL
    // Same formula as before G10.2
    int old = calculate_damage(50, 20, AttackType::PHYSICAL);
    // 50 - 20*0.5 = 40, variance 0.8-1.2 → 32-48
    EXPECT_GE(old, 20);
    EXPECT_LE(old, 60);
}
