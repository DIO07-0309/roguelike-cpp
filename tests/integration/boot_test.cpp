// G7.2: Integration boot test — loads all registries, exits with 0
#include <gtest/gtest.h>
#include <string>

#include "combat_system.h"   // load_buff_defs / load_relic_defs
#include "data/enemy_defs.h"
#include "data/boss_defs.h"
#include "data/skill_defs.h"
#include "data/item_defs.h"
#include "biome.h"
#include "landmark.h"
#include "encounter.h"

TEST(IntegrationBoot, GameplayRegistry) {
    EXPECT_TRUE(load_buff_defs("resources/buffs.json"));
    EXPECT_TRUE(load_relic_defs("resources/relics.json"));
    EXPECT_TRUE(load_enemy_defs("resources/enemies.json"));
    EXPECT_TRUE(load_boss_defs("resources/bosses.json"));
    EXPECT_TRUE(load_skill_defs("resources/skills.json"));
    EXPECT_TRUE(load_item_defs("resources/items.json"));
}

TEST(IntegrationBoot, WorldRegistry) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_landmark_defs("resources/landmarks.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
}

TEST(IntegrationBoot, FullBootstrapNoCrash) {
    EXPECT_TRUE(load_buff_defs("resources/buffs.json"));
    EXPECT_TRUE(load_relic_defs("resources/relics.json"));
    EXPECT_TRUE(load_enemy_defs("resources/enemies.json"));
    EXPECT_TRUE(load_boss_defs("resources/bosses.json"));
    EXPECT_TRUE(load_skill_defs("resources/skills.json"));
    EXPECT_TRUE(load_item_defs("resources/items.json"));
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_landmark_defs("resources/landmarks.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
}