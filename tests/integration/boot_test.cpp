// G7.2: Integration boot test — loads all registries, exits with 0
#include <gtest/gtest.h>
#include <string>

// Data loaders (const string refs)
extern bool load_buff_defs(const std::string&);
extern bool load_relic_defs(const std::string&);
extern bool load_enemy_defs(const std::string&);
extern bool load_boss_defs(const std::string&);
extern bool load_skill_defs(const std::string&);
extern bool load_item_defs(const std::string&);

// World loaders (const char*)
extern bool load_biome_defs(const char*);
extern bool load_landmark_defs(const char*);
extern bool load_hazard_defs(const char*);
extern bool load_encounter_defs(const char*);
extern bool load_biome_event_defs(const char*);

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
    EXPECT_TRUE(load_hazard_defs("resources/hazards.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    EXPECT_TRUE(load_biome_event_defs("resources/biome_events.json"));
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
    EXPECT_TRUE(load_hazard_defs("resources/hazards.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    EXPECT_TRUE(load_biome_event_defs("resources/biome_events.json"));
}
