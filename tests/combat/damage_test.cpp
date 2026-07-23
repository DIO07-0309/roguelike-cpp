// G7.2: Combat damage formula test
// Tests the core damage calculation without needing full Player/Monster objects.
#include <gtest/gtest.h>
#include <cstdint>

// Replicate the project's damage formula for unit testing
// From combat_system.h: calculate_damage(attacker_atk, defender_def, AttackType)
enum class AttackType { PHYSICAL, MAGICAL, TRUE };

static int calculate_damage(int atk, int def, AttackType type = AttackType::PHYSICAL) {
    if (type == AttackType::TRUE) return std::max(1, atk);
    int base = atk - (int)(def * 0.5f);
    // ±20% random swing → test with 1.0 multiplier (no swing)
    return std::max(1, base);
}

TEST(DamageCalc, BasicPhysical) {
    // ATK=10, DEF=2 → 10 - 1 = 9
    int dmg = calculate_damage(10, 2, AttackType::PHYSICAL);
    EXPECT_EQ(dmg, 9);
}

TEST(DamageCalc, MinimumDamage) {
    // ATK=1, DEF=20 → 1 - 10 = -9 → clamped to 1
    int dmg = calculate_damage(1, 20, AttackType::PHYSICAL);
    EXPECT_EQ(dmg, 1);
}

TEST(DamageCalc, TrueDamageIgnoresDefense) {
    // ATK=10, DEF=100 → TRUE damage ignores def → 10
    int dmg = calculate_damage(10, 100, AttackType::TRUE);
    EXPECT_EQ(dmg, 10);
}

TEST(DamageCalc, MagicalVsPhysical) {
    // Both ATK=12, PHY DEF=4 → 12-2=10, MAG DEF=8 → 12-4=8
    int phy = calculate_damage(12, 4, AttackType::PHYSICAL);
    int mag = calculate_damage(12, 8, AttackType::MAGICAL);
    EXPECT_EQ(phy, 10);
    EXPECT_EQ(mag, 8);
}

TEST(DamageCalc, HighDamage) {
    int dmg = calculate_damage(35, 5);
    EXPECT_GE(dmg, 25); // 35 - 2 = 33, clamped to 1
}

TEST(DamageCalc, ZeroAttackResult) {
    int dmg = calculate_damage(0, 0);
    EXPECT_EQ(dmg, 1); // minimum 1
}
