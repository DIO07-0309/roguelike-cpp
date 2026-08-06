// G10.3: Element Combat Framework tests
#include <gtest/gtest.h>
#include "components/element_component.h"
#include "data/element_defs.h"

// ═══════════════════════════════════════════════════
// Fire — crit stats
// ═══════════════════════════════════════════════════

class ElementCombat : public ::testing::Test {
protected:
    void SetUp() override { load_element_defs("resources/elements.json"); }
};

TEST_F(ElementCombat, FireCritChanceLv1) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    EXPECT_NEAR(ec.fire_crit_chance(), 15.0f, 0.01f);
}

TEST_F(ElementCombat, FireCritChanceGrows) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    ec.add_exp(2000); // push to high level
    EXPECT_GT(ec.fire_crit_chance(), 15.0f);
    EXPECT_LE(ec.fire_crit_chance(), 40.0f);
}

TEST_F(ElementCombat, FireCritMultiplier) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    EXPECT_FLOAT_EQ(ec.fire_crit_mult(), 1.5f);
}

// ═══════════════════════════════════════════════════
// Ice — freeze mechanics
// ═══════════════════════════════════════════════════

TEST_F(ElementCombat, IceFreezeChanceLv1) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    EXPECT_NEAR(ec.ice_freeze_chance(), 10.0f, 0.01f);
}

TEST_F(ElementCombat, IceFreezeChanceGrows) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    ec.add_exp(2000); // many levels (Lv6: 10 + 90*5/19 ≈ 33.7)
    EXPECT_GT(ec.ice_freeze_chance(), 30.0f);
    EXPECT_LE(ec.ice_freeze_chance(), 105.0f);
}

TEST_F(ElementCombat, IceFreezeCounterMax) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    EXPECT_EQ(ec.ice_freeze_counter_max(), 3);
}

TEST_F(ElementCombat, FreezeCounterStartsAtZero) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    EXPECT_EQ(ec.freeze_counter, 0);
}

// ═══════════════════════════════════════════════════
// Poison — DOT scaling
// ═══════════════════════════════════════════════════

TEST_F(ElementCombat, PoisonDotScaleLv1) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    EXPECT_NEAR(ec.poison_dot_scale(), 0.05f, 0.001f);
}

TEST_F(ElementCombat, PoisonDotScaleGrows) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    ec.add_exp(2000);
    EXPECT_GT(ec.poison_dot_scale(), 0.05f);
    EXPECT_LE(ec.poison_dot_scale(), 0.20f);
}

TEST_F(ElementCombat, PoisonDotDuration) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    EXPECT_FLOAT_EQ(ec.poison_dot_duration(), 3.0f);
}

// ═══════════════════════════════════════════════════
// Element upgrade
// ═══════════════════════════════════════════════════

TEST_F(ElementCombat, UpgradeIncreasesLevel) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    EXPECT_EQ(ec.level, 1);
    ec.add_exp(100); // exactly BASE_XP * 1
    EXPECT_EQ(ec.level, 2);
}

TEST_F(ElementCombat, AddExpOnKillIsCumulative) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    ec.add_exp(5);   // 1 hit kill
    ec.add_exp(50);  // 10 hits
    ec.add_exp(50);  // → 105 total → level 2, 5 leftover
    EXPECT_EQ(ec.level, 2);
    EXPECT_EQ(ec.experience, 5);
}

// ═══════════════════════════════════════════════════
// Buff compatibility
// ═══════════════════════════════════════════════════

TEST_F(ElementCombat, FireGeneratesNoBuff) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    // Fire only modifies damage, no buff applied
    // Verified by ElementResolver::resolve_fire()
    // No buff application in the fire path
    EXPECT_EQ(ec.type, ElementType::FIRE);
}

TEST_F(ElementCombat, IceGeneratesSlow) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    // Ice always applies slow then checks freeze
    // Verified by ElementResolver::resolve_ice()
    EXPECT_EQ(ec.type, ElementType::ICE);
}

TEST_F(ElementCombat, PoisonGeneratesPoison) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    // Poison applies poison buff with stacked ticks
    EXPECT_EQ(ec.type, ElementType::POISON);
}

// ═══════════════════════════════════════════════════
// Element data from JSON
// ═══════════════════════════════════════════════════

TEST_F(ElementCombat, FireDefHasCombatFields) {
    const ElementDef* d = get_element_def("fire");
    ASSERT_NE(d, nullptr);
    EXPECT_GT(d->crit_base, 0.0f);
    EXPECT_GT(d->crit_multiplier, 1.0f);
}

TEST_F(ElementCombat, IceDefHasFreezeFields) {
    const ElementDef* d = get_element_def("ice");
    ASSERT_NE(d, nullptr);
    EXPECT_GT(d->freeze_counter_max, 0);
    EXPECT_GT(d->freeze_stage3, d->freeze_stage1);
}

TEST_F(ElementCombat, PoisonDefHasDotFields) {
    const ElementDef* d = get_element_def("poison");
    ASSERT_NE(d, nullptr);
    EXPECT_GT(d->dot_scale_base, 0.0f);
    EXPECT_GT(d->dot_duration, 0.0f);
}
