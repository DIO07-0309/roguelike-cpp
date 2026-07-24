// G9.2: Equipment identity tests — naming, affix, legendary effects
#include <gtest/gtest.h>
#include <string>
#include "data/weapon_defs.h"
#include "types/weapon_types.h"

// ═══════════════════════════════════════════════════
// Naming tests
// ═══════════════════════════════════════════════════

class EquipmentNaming : public ::testing::Test {
protected:
    void SetUp() override { load_weapon_defs("resources/weapons.json"); }
};

TEST_F(EquipmentNaming, CommonUsesBaseName) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(pick_weapon_name(d, 0), "匕首");
}

TEST_F(EquipmentNaming, RarePicksFromPool) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    const char* name = pick_weapon_name(d, 1);
    // Should be one of the two rare names
    bool match = (std::string(name) == "暗影猎手" || std::string(name) == "血牙");
    EXPECT_TRUE(match) << "Got: " << name;
}

TEST_F(EquipmentNaming, EpicPicksFromPool) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    const char* name = pick_weapon_name(d, 2);
    bool match = (std::string(name) == "夜魔之刃" || std::string(name) == "深渊獠牙");
    EXPECT_TRUE(match) << "Got: " << name;
}

TEST_F(EquipmentNaming, LegendaryReturnsFixedName) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(pick_weapon_name(d, 3), "恶魔之爪");
}

TEST_F(EquipmentNaming, NullDefReturnsEmpty) {
    EXPECT_STREQ(pick_weapon_name(nullptr, 0), "");
}

TEST_F(EquipmentNaming, SwordLegendaryName) {
    const WeaponDef* d = get_weapon_def("sword_common");
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(pick_weapon_name(d, 3), "倚天剑");
}

TEST_F(EquipmentNaming, AllLegendaryNamesCorrect) {
    struct { const char* id; const char* legendary; } expect[] = {
        {"dagger_legendary", "恶魔之爪"},
        {"sword_legendary", "倚天剑"},
        {"nunchaku_legendary", "李小龙"},
        {"crossbow_legendary", "东风破"},
        {"spear_legendary", "惊破天"},
    };
    for (auto& e : expect) {
        const WeaponDef* d = get_weapon_def(e.id);
        ASSERT_NE(d, nullptr) << "Missing: " << e.id;
        EXPECT_STREQ(d->legendary_name.c_str(), e.legendary)
            << "Wrong legendary name for " << e.id;
    }
}

// ═══════════════════════════════════════════════════
// Quality tier tests
// ═══════════════════════════════════════════════════

TEST_F(EquipmentNaming, QualityTiersAreDistinct) {
    const WeaponDef* dagger_c = get_weapon_def("dagger_common");
    ASSERT_NE(dagger_c, nullptr);
    EXPECT_STREQ(dagger_c->rarity.c_str(), "common");

    const WeaponDef* dagger_r = get_weapon_def("dagger_rare");
    ASSERT_NE(dagger_r, nullptr);
    EXPECT_STREQ(dagger_r->rarity.c_str(), "rare");

    const WeaponDef* dagger_l = get_weapon_def("dagger_legendary");
    ASSERT_NE(dagger_l, nullptr);
    EXPECT_STREQ(dagger_l->rarity.c_str(), "legendary");
}

// ═══════════════════════════════════════════════════
// Affix reading tests
// ═══════════════════════════════════════════════════

TEST_F(EquipmentNaming, DaggerHasBleedAffix) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->affix.type, "bleed");
    EXPECT_GT(d->affix.value, 0.0f);
    EXPECT_LE(d->affix.value, 30.0f);
}

TEST_F(EquipmentNaming, SwordHasRangeBoostAffix) {
    const WeaponDef* d = get_weapon_def("sword_common");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->affix.type, "range_boost");
    EXPECT_FLOAT_EQ(d->affix.value, 0.5f);
}

TEST_F(EquipmentNaming, NunchakuHasDamageRampAffix) {
    const WeaponDef* d = get_weapon_def("nunchaku_common");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->affix.type, "damage_ramp");
    EXPECT_GT(d->affix.value, 0.05f);
}

TEST_F(EquipmentNaming, CrossbowHasCdReduceAffix) {
    const WeaponDef* d = get_weapon_def("crossbow_common");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->affix.type, "cd_reduce");
    EXPECT_GT(d->affix.value, 0.15f);
}

TEST_F(EquipmentNaming, SpearHasPierceBonusAffix) {
    const WeaponDef* d = get_weapon_def("spear_common");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->affix.type, "pierce_bonus");
    EXPECT_GT(d->affix.value, 0.1f);
}

// ═══════════════════════════════════════════════════
// Legendary effect tests
// ═══════════════════════════════════════════════════

TEST_F(EquipmentNaming, LegendaryHasEffect) {
    const WeaponDef* d = get_weapon_def("dagger_legendary");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->legendary_effect, "dagger_bleed");
}

TEST_F(EquipmentNaming, CommonHasNoLegendaryEffect) {
    const WeaponDef* d = get_weapon_def("dagger_common");
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->legendary_effect.empty());
}

TEST_F(EquipmentNaming, AllLegendaryEffectsSet) {
    struct { const char* id; const char* effect; } expect[] = {
        {"dagger_legendary", "dagger_bleed"},
        {"sword_legendary", "sword_wave"},
        {"nunchaku_legendary", "nunchaku_hits"},
        {"crossbow_legendary", "crossbow_power"},
        {"spear_legendary", "spear_count"},
    };
    for (auto& e : expect) {
        const WeaponDef* d = get_weapon_def(e.id);
        ASSERT_NE(d, nullptr) << "Missing: " << e.id;
        EXPECT_EQ(d->legendary_effect, e.effect)
            << "Wrong effect for " << e.id;
        EXPECT_EQ(d->rarity, "legendary") << e.id << " should be legendary";
    }
}

TEST_F(EquipmentNaming, NonLegendaryHaveNoEffect) {
    const char* non_legendary[] = {
        "dagger_common", "dagger_rare", "dagger_epic",
        "sword_common", "sword_rare", "sword_epic",
        "nunchaku_common", "nunchaku_rare", "nunchaku_epic",
        "crossbow_common", "crossbow_rare", "crossbow_epic",
        "spear_common", "spear_rare", "spear_epic",
    };
    for (auto id : non_legendary) {
        const WeaponDef* d = get_weapon_def(id);
        ASSERT_NE(d, nullptr) << "Missing: " << id;
        EXPECT_TRUE(d->legendary_effect.empty())
            << id << " has unexpected legendary effect: " << d->legendary_effect;
    }
}

// ═══════════════════════════════════════════════════
// Affix value progression tests (higher tier = stronger)
// ═══════════════════════════════════════════════════

TEST_F(EquipmentNaming, AffixScalesWithQuality) {
    // Dagger bleed: common=15, rare=15, epic=20, legendary=25
    float vals[4];
    for (int i = 0; i < 4; ++i) {
        const char* ids[] = {"dagger_common","dagger_rare","dagger_epic","dagger_legendary"};
        const WeaponDef* d = get_weapon_def(ids[i]);
        ASSERT_NE(d, nullptr);
        vals[i] = d->affix.value;
    }
    EXPECT_GE(vals[2], vals[1]); // epic >= rare
    EXPECT_GE(vals[3], vals[2]); // legendary >= epic
}

TEST_F(EquipmentNaming, All24WeaponsHaveValidAffix) {
    auto& all = get_all_weapon_defs();
    for (auto& kv : all) {
        const WeaponDef* d = &kv.second;
        if (d->type == WeaponType::FIST) {
            EXPECT_TRUE(d->affix.type.empty()) << "FIST should have empty affix";
        } else {
            EXPECT_FALSE(d->affix.type.empty())
                << d->id << " has empty affix type";
        }
    }
}
