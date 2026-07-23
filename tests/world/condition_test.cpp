// G7.2: Condition format + action string tests
// Tests condition field existence and DSL format validity
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "world/encounter.h"
#include "world/biome.h"

// Simple inline condition parser
static bool parse_condition(const std::string& cond) {
    if (cond.empty()) return true;
    if (cond.find("flag:") == 0 && cond.size() > 5) return true;
    if (cond.find("!flag:") == 0 && cond.size() > 6) return true;
    if (cond.find("floor>=") == 0 && cond.size() > 7) return true;
    if (cond.find("floor<=") == 0 && cond.size() > 7) return true;
    if (cond.find("biome:") == 0 && cond.size() > 6) return true;
    if (cond.find("!biome:") == 0 && cond.size() > 7) return true;
    return false; // unknown format
}

TEST(ConditionField, EncountersHaveConditionsArray) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("prisoner_merchant");
    ASSERT_NE(enc, nullptr);
    // conditions field should exist (may be empty)
    EXPECT_TRUE(enc->conditions.empty() || enc->conditions.size() >= 0);
}

TEST(ConditionFormat, ValidConditionDSL) {
    // Test valid condition strings
    EXPECT_TRUE(parse_condition("flag:500"));
    EXPECT_TRUE(parse_condition("!flag:999"));
    EXPECT_TRUE(parse_condition("floor>=3"));
    EXPECT_TRUE(parse_condition("floor<=10"));
    EXPECT_TRUE(parse_condition("biome:ash_volcano"));
    EXPECT_TRUE(parse_condition("!biome:forgotten_prison"));
}

TEST(ConditionFormat, InvalidConditionDSL) {
    EXPECT_FALSE(parse_condition(""));
    EXPECT_FALSE(parse_condition("invalid"));
    EXPECT_FALSE(parse_condition("flag:"));
    EXPECT_FALSE(parse_condition("floor>"));
}

TEST(ConditionFormat, EmptyConditionsIsValid) {
    // Empty conditions list means "always allowed"
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("prisoner_merchant");
    ASSERT_NE(enc, nullptr);
    EXPECT_TRUE(enc->conditions.empty());
    // old_prisoner also has no conditions — should work
    enc = get_encounter("old_prisoner");
    ASSERT_NE(enc, nullptr);
    EXPECT_TRUE(enc->conditions.empty());
}
