// G10.1: Element Core unit tests
#include <gtest/gtest.h>
#include "components/element_component.h"
#include "data/element_defs.h"

// ═══════════════════════════════════════════════════
// ElementComponent
// ═══════════════════════════════════════════════════

TEST(ElementComponent, DefaultIsNONE) {
    ElementComponent ec;
    EXPECT_EQ(ec.type, ElementType::NONE);
    EXPECT_EQ(ec.level, 1);
    EXPECT_EQ(ec.experience, 0);
    EXPECT_FALSE(ec.initialized);
}

TEST(ElementComponent, SelectFire) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    EXPECT_EQ(ec.type, ElementType::FIRE);
    EXPECT_TRUE(ec.initialized);
    EXPECT_EQ(ec.level, 1);
    EXPECT_EQ(ec.experience, 0);
}

TEST(ElementComponent, SelectIce) {
    ElementComponent ec;
    ec.select(ElementType::ICE);
    EXPECT_EQ(ec.type, ElementType::ICE);
    EXPECT_TRUE(ec.initialized);
}

TEST(ElementComponent, SelectPoison) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    EXPECT_EQ(ec.type, ElementType::POISON);
    EXPECT_TRUE(ec.initialized);
}

TEST(ElementComponent, ReselectOverwrites) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    ec.select(ElementType::ICE);
    EXPECT_EQ(ec.type, ElementType::ICE);
    EXPECT_TRUE(ec.initialized);
}

TEST(ElementComponent, XpDoesNothingIfNotInitialized) {
    ElementComponent ec;
    ec.add_exp(500);
    EXPECT_EQ(ec.level, 1);
    EXPECT_EQ(ec.experience, 0);
}

TEST(ElementComponent, AddExpLevelsUp) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    ec.add_exp(100); // BASE_XP * 1
    EXPECT_EQ(ec.level, 2);
    EXPECT_EQ(ec.experience, 0);
}

TEST(ElementComponent, AddExpAccumulates) {
    ElementComponent ec;
    ec.select(ElementType::FIRE);
    ec.add_exp(50);
    EXPECT_EQ(ec.level, 1);
    EXPECT_EQ(ec.experience, 50);
    ec.add_exp(70); // 120 total → level 2, 20 leftover
    EXPECT_EQ(ec.level, 2);
    EXPECT_EQ(ec.experience, 20);
}

TEST(ElementComponent, MultiLevelUp) {
    ElementComponent ec;
    ec.select(ElementType::POISON);
    ec.add_exp(650); // 100+200+300 = 600 → level 4
    EXPECT_EQ(ec.level, 4);
    EXPECT_EQ(ec.experience, 50);
}

// ═══════════════════════════════════════════════════
// ElementType string conversion
// ═══════════════════════════════════════════════════

TEST(ElementType, NameConversion) {
    EXPECT_STREQ(element_type_name(ElementType::FIRE), "fire");
    EXPECT_STREQ(element_type_name(ElementType::ICE), "ice");
    EXPECT_STREQ(element_type_name(ElementType::POISON), "poison");
    EXPECT_STREQ(element_type_name(ElementType::NONE), "none");
}

TEST(ElementType, FromString) {
    EXPECT_EQ(element_from_string("fire"), ElementType::FIRE);
    EXPECT_EQ(element_from_string("ice"), ElementType::ICE);
    EXPECT_EQ(element_from_string("poison"), ElementType::POISON);
    EXPECT_EQ(element_from_string("invalid"), ElementType::NONE);
    EXPECT_EQ(element_from_string(nullptr), ElementType::NONE);
}

// ═══════════════════════════════════════════════════
// ElementDef data loading
// ═══════════════════════════════════════════════════

TEST(ElementDef, LoadFromJSON) {
    EXPECT_TRUE(load_element_defs("resources/elements.json"));
    EXPECT_TRUE(is_element_defs_loaded());
}

TEST(ElementDef, FireDef) {
    load_element_defs("resources/elements.json");
    const ElementDef* d = get_element_def("fire");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->id, "fire");
    EXPECT_EQ(d->name, "火焰核心");
    EXPECT_GT(d->crit_base, 0);
}

TEST(ElementDef, IceDef) {
    load_element_defs("resources/elements.json");
    const ElementDef* d = get_element_def("ice");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->name, "冰霜核心");
    EXPECT_GT(d->freeze_counter_max, 0);
}

TEST(ElementDef, PoisonDef) {
    load_element_defs("resources/elements.json");
    const ElementDef* d = get_element_def("poison");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->name, "剧毒核心");
    EXPECT_GT(d->dot_scale_base, 0);
}

TEST(ElementDef, AllThreeLoaded) {
    load_element_defs("resources/elements.json");
    auto& all = get_all_element_defs();
    EXPECT_GE((int)all.size(), 3);
}
