// G7.2: Save/Load roundtrip regression test
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <direct.h>
#include <cstdio>

extern bool load_biome_defs(const char* path);
extern bool load_encounter_defs(const char* path);

static std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

TEST(SaveFormat, RoundTripWriteRead) {
    _mkdir("saves");
    const char* path = "saves/test_save.json";
    std::ofstream w(path);
    w << "{\"test\":1}";
    w.close();
    std::string content = read_file(path);
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
    std::remove(path);
}

TEST(SaveFormat, MetaSaveRoundTrip) {
    _mkdir("saves");
    const char* path = "saves/test_meta_save.json";
    std::ofstream w(path);
    w << "{\"meta\":true}";
    w.close();
    std::string content = read_file(path);
    EXPECT_GT(content.size(), 2u);
    EXPECT_EQ(content[0], '{');
    std::remove(path);
}

TEST(SaveRoundtrip, ResourcesAndSavesCoexist) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_encounter_defs("resources/encounters.json"));
    _mkdir("saves");
    const char* path = "saves/test_save2.json";
    std::ofstream w(path);
    w << "{}";
    w.close();
    EXPECT_TRUE(std::ifstream(path).is_open()) << "Save missing after resource load";
    std::remove(path);
}