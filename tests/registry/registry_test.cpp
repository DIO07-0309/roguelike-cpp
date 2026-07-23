// G7.2: Registry loader + reference integrity tests
#include <gtest/gtest.h>
#include <string>

// Include actual loader headers (no raylib deps needed for these)
extern bool load_buff_defs(const std::string& json_path);
extern bool load_relic_defs(const std::string& json_path);
extern bool load_enemy_defs(const std::string& json_path);
extern bool load_boss_defs(const std::string& json_path);
extern bool load_skill_defs(const std::string& json_path);
extern bool load_item_defs(const std::string& json_path);

// World loaders (const char*)
extern bool load_biome_defs(const char* json_path);
extern bool load_landmark_defs(const char* json_path);
extern bool load_hazard_defs(const char* json_path);
extern bool load_encounter_defs(const char* json_path);
extern bool load_biome_event_defs(const char* json_path);

// World queries
struct BiomeDef { std::string id, name; int floor_start, floor_end; };
extern const BiomeDef* get_biome_for_floor(int floor);

struct LandmarkDef { std::string id, icon; };
extern std::vector<const LandmarkDef*> get_landmarks_for_biome(const std::string&);

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

TEST(RegistryLoad, BiomeEvents) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_biome_event_defs("resources/biome_events.json"));
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
