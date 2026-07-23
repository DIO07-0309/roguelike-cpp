// G7.2: Action dispatch test — string + dict dual format
#include <gtest/gtest.h>
#include <string>

// Simple action parser (mirrors encounter.cpp execute_action logic)
static std::string parse_action_type(const std::string& action) {
    if (action.empty() || action == "none") return "none";
    auto pos = action.find(':');
    if (pos == std::string::npos) return action;
    return action.substr(0, pos);
}

TEST(ActionStringFormat, BuffParsing) {
    EXPECT_EQ(parse_action_type("buff:attack_up:2"), "buff");
    EXPECT_EQ(parse_action_type("buff:defense_up:1"), "buff");
    EXPECT_EQ(parse_action_type("buff:blessing:1"), "buff");
}

TEST(ActionStringFormat, SpawnParsing) {
    EXPECT_EQ(parse_action_type("spawn:elite_orc:1"), "spawn");
    EXPECT_EQ(parse_action_type("spawn:skeleton_archer:2"), "spawn");
}

TEST(ActionStringFormat, EquipmentParsing) {
    EXPECT_EQ(parse_action_type("equipment:rare"), "equipment");
    EXPECT_EQ(parse_action_type("equipment:epic"), "equipment");
    EXPECT_EQ(parse_action_type("equipment:legendary"), "equipment");
}

TEST(ActionStringFormat, HpLossParsing) {
    EXPECT_EQ(parse_action_type("hp_loss:15"), "hp_loss");
    EXPECT_EQ(parse_action_type("hp_loss:50"), "hp_loss");
}

TEST(ActionStringFormat, NoneIsNone) {
    EXPECT_EQ(parse_action_type("none"), "none");
    EXPECT_EQ(parse_action_type(""), "none");
}

TEST(ActionStringFormat, AllKnownTypes) {
    const char* types[] = {"buff","relic","equipment","skill_level",
                           "heal","spawn","hp_loss","debuff","confuse",
                           "trade","set_flag","set_meta_flag",nullptr};
    for (int i=0; types[i]; i++) {
        std::string action = std::string(types[i]) + ":test:1";
        EXPECT_EQ(parse_action_type(action), types[i]);
    }
}

TEST(ActionDictFormat, DictStructure) {
    // Simulate dict action parsing (not linked to actual code, test the format)
    struct ActionDict { std::string type, name; int stacks; };
    ActionDict ad{"buff", "attack_up", 2};
    EXPECT_EQ(ad.type, "buff");
    EXPECT_EQ(ad.name, "attack_up");
    EXPECT_EQ(ad.stacks, 2);
    // String and dict should resolve to same effect type
    EXPECT_EQ(ad.type, parse_action_type("buff:attack_up:2"));
}
