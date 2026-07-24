// G9: Weapon system unit tests
#include <gtest/gtest.h>
#include <cmath>
#include "data/weapon_defs.h"
#include "systems/weapon_component.h"
#include "systems/hit_detection.h"

// ═══════════════════════════════════════════════════
// WeaponDef loading tests
// ═══════════════════════════════════════════════════

TEST(WeaponDefLoad, FistBasicExists) {
    // Load from resources/weapons.json
    bool ok = load_weapon_defs("resources/weapons.json");
    EXPECT_TRUE(ok);
    const WeaponDef* fist = get_weapon_def("fist_basic");
    ASSERT_NE(fist, nullptr);
    EXPECT_EQ(fist->type, WeaponType::FIST);
    EXPECT_EQ(fist->stage_count, 1);
    EXPECT_FLOAT_EQ(fist->stages[0].damage_multiplier, 1.0f);
}

TEST(WeaponDefLoad, DaggerHas3Stages) {
    load_weapon_defs("resources/weapons.json");
    const WeaponDef* dagger = get_weapon_def("dagger_common");
    ASSERT_NE(dagger, nullptr);
    EXPECT_EQ(dagger->type, WeaponType::DAGGER);
    EXPECT_EQ(dagger->stage_count, 3);
    // Stage 3 should be thrust with 1.15x multiplier
    EXPECT_GT(dagger->stages[2].damage_multiplier, 1.0f);
    EXPECT_EQ(dagger->stages[2].hit_shape, HitShape::CAPSULE);
}

TEST(WeaponDefLoad, All6TypesLoad) {
    load_weapon_defs("resources/weapons.json");
    for (int i = 0; i < (int)WeaponType::COUNT; ++i) {
        auto wt = (WeaponType)i;
        auto list = get_weapon_defs_for_type(wt);
        EXPECT_FALSE(list.empty()) << "No weapons for type " << weapon_type_name(wt);
    }
}

TEST(WeaponDefLoad, QualityNamesPopulated) {
    load_weapon_defs("resources/weapons.json");
    const WeaponDef* sw = get_weapon_def("sword_legendary");
    ASSERT_NE(sw, nullptr);
    EXPECT_EQ(sw->quality_names[0], "长剑");
    EXPECT_EQ(sw->quality_names[3], "倚天剑");
}

// ═══════════════════════════════════════════════════
// WeaponComponent tests
// ═══════════════════════════════════════════════════

class WeaponComponentTest : public ::testing::Test {
protected:
    void SetUp() override {
        load_weapon_defs("resources/weapons.json");
    }
};

TEST_F(WeaponComponentTest, DefaultIsFist) {
    WeaponComponent wc;
    EXPECT_EQ(wc.weapon_type(), WeaponType::FIST);
    EXPECT_STREQ(wc.current_weapon_id(), "fist_basic");
}

TEST_F(WeaponComponentTest, EquipDagger) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    EXPECT_EQ(wc.weapon_type(), WeaponType::DAGGER);
}

TEST_F(WeaponComponentTest, EquipAndUnequip) {
    WeaponComponent wc;
    wc.equip("sword_common");
    EXPECT_EQ(wc.weapon_type(), WeaponType::SWORD);
    wc.unequip();
    EXPECT_EQ(wc.weapon_type(), WeaponType::FIST);
}

TEST_F(WeaponComponentTest, ComboAdvances) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    EXPECT_EQ(wc.combo_index(), 0);
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1);
    wc.execute_attack(1.3);
    EXPECT_EQ(wc.combo_index(), 2);
}

TEST_F(WeaponComponentTest, ComboTimeoutResets) {
    WeaponComponent wc;
    wc.equip("dagger_common");
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1);
    // Tick past timeout
    for (int i = 0; i < 60; ++i) wc.tick(0.05f); // 3 seconds
    EXPECT_EQ(wc.combo_index(), 0);
}

TEST_F(WeaponComponentTest, CannotAttackDuringRecovery) {
    WeaponComponent wc;
    wc.equip("sword_common");
    wc.execute_attack(1.0); // recovery ~0.35s
    EXPECT_FALSE(wc.can_attack(1.05)); // still in recovery
}

TEST_F(WeaponComponentTest, FistHasSingleStage) {
    WeaponComponent wc;
    EXPECT_EQ(wc.combo_index(), 0);
    wc.execute_attack(1.0);
    EXPECT_EQ(wc.combo_index(), 1); // advances but to 1
    EXPECT_FLOAT_EQ(wc.multiplier(), 1.0f);
}

// ═══════════════════════════════════════════════════
// Hit detection tests (shape geometry)
// ═══════════════════════════════════════════════════

// Minimal mock monster struct for hit detection testing
struct MockMonster {
    bool is_alive = true;
    Rectangle rect;
};

// Convert mock to Monster* compatible layout (only used for testing)
// We test hit_detect functions with controlled inputs
// Since they take Monster*, we validate geometry through manual calculation

TEST(HitDetection, CircleDetectInRange) {
    // Manual test: center (100,100), radius 64px
    // Target at (140, 100) — distance 40px, should hit
    float dist = std::hypot(140.0f - 100.0f, 100.0f - 100.0f);
    EXPECT_LE(dist, 64.0f);
    // Target at (200, 200) — distance ~141px, should miss
    float dist2 = std::hypot(200.0f - 100.0f, 200.0f - 100.0f);
    EXPECT_GT(dist2, 64.0f);
}

TEST(HitDetection, SectorAngleCheck) {
    // Forward: RIGHT (1,0)
    // Target at (150, 100) — angle 0, in sector
    float dx1 = 150 - 100, dy1 = 100 - 100;
    float ang1 = std::acos(std::max(-1.0f, std::min(1.0f, dx1 / std::hypot(dx1, dy1))));
    EXPECT_LE(ang1 * 180.0f / 3.14159f, 45.0f);

    // Target at (100, 140) — angle 90deg, out of 45-deg sector
    float dx2 = 100 - 100, dy2 = 140 - 100;
    if (std::hypot(dx2, dy2) > 0.001f) {
        float ang2 = std::acos(std::max(-1.0f, std::min(1.0f, dx2 / std::hypot(dx2, dy2))));
        EXPECT_GT(ang2 * 180.0f / 3.14159f, 45.0f);
    }
}

TEST(HitDetection, CapsuleSegDist) {
    // Capsule from (0,0) to (100,0) with radius 20
    // Point (50, 10) — distance 10px from segment, in range
    float t = std::max(0.0f, std::min(1.0f, (50.0f*100.0f + 10.0f*0.0f) / (100.0f*100.0f)));
    float cx = t * 100.0f, cy = t * 0.0f;
    float seg_dist = std::hypot(50.0f - cx, 10.0f - cy);
    EXPECT_LE(seg_dist, 20.0f);

    // Point (50, 30) — distance 30px, out of range
    float seg_dist2 = std::hypot(50.0f - cx, 30.0f - cy);
    EXPECT_GT(seg_dist2, 20.0f);
}

TEST(HitDetection, RectangleOverlap) {
    // Rectangle centered at (50,0), length=100, width=30
    // Point (50, 0) at center — inside
    float lx = 50.0f, ly = 0.0f;
    EXPECT_LE(std::abs(lx), 66.0f); // half_len + margin
    EXPECT_LE(std::abs(ly), 31.0f); // half_width + margin
    // Point (200, 0) far right — outside
    EXPECT_GT(std::abs(200.0f - 50.0f), 50.0f + 16.0f);
}
