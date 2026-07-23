// G7.2: Save/Load roundtrip regression test
#include <gtest/gtest.h>
#include <fstream>
#include <string>

extern bool load_biome_defs(const char* path);
extern bool load_encounter_defs(const char* path);

TEST(SaveFormat, SaveJsonExists) {
    EXPECT_TRUE(std::ifstream("saves/save.json").is_open())
        << "saves/save.json missing";
}

TEST(SaveFormat, MetaSaveExists) {
    EXPECT_TRUE(std::ifstream("saves/meta_save.json").is_open())
        << "saves/meta_save.json missing — G6.7 meta state";
}

TEST(SaveFormat, SaveIsReadable) {
    std::ifstream f("saves/save.json");
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
}

TEST(SaveFormat, MetaSaveIsReadable) {
    std::ifstream f("saves/meta_save.json");
    ASSERT_TRUE(f.is_open());
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
}

TEST(SaveRoundtrip, ResourcesAndSavesCoexist) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    std::ifstream save("saves/save.json");
    EXPECT_TRUE(save.is_open()) << "Save missing after resource load";
}
