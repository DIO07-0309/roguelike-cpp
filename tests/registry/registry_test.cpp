// G7.2: Registry loader + reference integrity tests
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "combat_system.h"   // load_buff_defs / load_relic_defs
#include "data/enemy_defs.h"
#include "data/boss_defs.h"
#include "data/skill_defs.h"
#include "data/item_defs.h"
#include "biome.h"           // load_biome_defs / BiomeDef / get_biome_for_floor
#include "landmark.h"        // load_landmark_defs / get_landmarks_for_biome
#include "hazard.h"          // load_hazard_defs
#include "encounter.h"       // load_encounter_defs

// ── Loader Tests ────────────────────────────────────────────

TEST(RegistryLoad, BuffsLoad)   { EXPECT_TRUE(load_buff_defs("resources/buffs.json")); }
TEST(RegistryLoad, RelicsLoad)  { EXPECT_TRUE(load_relic_defs("resources/relics.json")); }
TEST(RegistryLoad, EnemiesLoad) { EXPECT_TRUE(load_enemy_defs("resources/enemies.json")); }
TEST(RegistryLoad, BossesLoad)  { EXPECT_TRUE(load_boss_defs("resources/bosses.json")); }
TEST(RegistryLoad, SkillsLoad)  { EXPECT_TRUE(load_skill_defs("resources/skills.json")); }
TEST(RegistryLoad, ItemsLoad)   { EXPECT_TRUE(load_item_defs("resources/items.json")); }

TEST(RegistryLoad, WorldLayerAll) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_landmark_defs("resources/landmarks.json"));
    EXPECT_TRUE(load_hazard_defs("resources/hazards.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
}

// ── Reference Integrity ──────────────────────────────────────

TEST(ReferenceIntegrity, FloorBiomeMapping) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_EQ(get_biome_for_floor(1)->id, "forgotten_prison");
    EXPECT_EQ(get_biome_for_floor(7)->id, "ash_volcano");
    EXPECT_EQ(get_biome_for_floor(14)->id, "void_abyss");
}

TEST(ReferenceIntegrity, FifteenFloorsMapped) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    for (int f = 1; f <= 15; f++)
        EXPECT_NE(get_biome_for_floor(f), nullptr) << "Floor " << f << " unmapped";
}

TEST(ReferenceIntegrity, LandmarksPerBiome) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_landmark_defs("resources/landmarks.json"));
    EXPECT_GE(get_landmarks_for_biome("forgotten_prison").size(), 1u);
    EXPECT_GE(get_landmarks_for_biome("ash_volcano").size(), 1u);
    EXPECT_GE(get_landmarks_for_biome("void_abyss").size(), 1u);
}