// G7.2: Encounter dialogue integrity test
#include <gtest/gtest.h>
#include "world/encounter.h"
#include "world/biome.h"
#include "world/landmark.h"

TEST(EncounterIntegrity, PrisonerMerchantLoaded) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("prisoner_merchant");
    ASSERT_NE(enc, nullptr);
    EXPECT_EQ(enc->type, "npc");
    EXPECT_TRUE(enc->repeatable);
}

TEST(EncounterIntegrity, DialogueTreeComplete) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("prisoner_merchant");
    ASSERT_NE(enc, nullptr);

    bool has_greet=false, has_shop=false, has_farewell=false, has_end=false;
    for (auto& n : enc->dialogue) {
        if (n.id=="greet") has_greet=true;
        if (n.id=="shop") has_shop=true;
        if (n.id=="farewell") has_farewell=true;
        if (n.id=="end") has_end=true;
    }
    EXPECT_TRUE(has_greet);
    EXPECT_TRUE(has_shop);
    EXPECT_TRUE(has_farewell);
    EXPECT_TRUE(has_end);
}

TEST(EncounterIntegrity, NoBrokenNextLinks) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("prisoner_merchant");
    ASSERT_NE(enc, nullptr);
    std::vector<std::string> valid;
    for (auto& n : enc->dialogue) valid.push_back(n.id);
    for (auto& n : enc->dialogue)
        for (auto& c : n.choices)
            if (c.next != "end") {
                bool found = false;
                for (auto& v : valid) if (v == c.next) { found=true; break; }
                EXPECT_TRUE(found) << "Node '" << n.id << "' → '" << c.next << "' broken";
            }
}

TEST(EncounterIntegrity, AllSixNpcsLoad) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    const char* ids[]={"prisoner_merchant","old_prisoner","forge_master",
                       "lost_miner","watcher","void_trader",nullptr};
    for (int i=0; ids[i]; i++) {
        auto* enc = get_encounter(ids[i]);
        ASSERT_NE(enc,nullptr) << ids[i];
        EXPECT_GE(enc->dialogue.size(), 2u);
    }
}

TEST(EncounterIntegrity, SecretsAreWallInteract) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    auto* enc = get_encounter("hidden_prison_vault");
    ASSERT_NE(enc, nullptr);
    EXPECT_EQ(enc->trigger, "wall_interact");
    EXPECT_FALSE(enc->repeatable);
}
