// G7.2: Biome isolation + floor mapping tests
#include <gtest/gtest.h>
#include "world/biome.h"
#include "world/landmark.h"

TEST(BiomeIsolation, PrisonFloors1to5) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    for (int f=1; f<=5; f++) {
        const auto* b = get_biome_for_floor(f);
        ASSERT_NE(b, nullptr) << "Floor " << f;
        EXPECT_EQ(b->id, "forgotten_prison");
    }
}

TEST(BiomeIsolation, VolcanoFloors6to10) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    for (int f=6; f<=10; f++) {
        const auto* b = get_biome_for_floor(f);
        ASSERT_NE(b, nullptr) << "Floor " << f;
        EXPECT_EQ(b->id, "ash_volcano");
    }
}

TEST(BiomeIsolation, AbyssFloors11to15) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    for (int f=11; f<=15; f++) {
        const auto* b = get_biome_for_floor(f);
        ASSERT_NE(b, nullptr) << "Floor " << f;
        EXPECT_EQ(b->id, "void_abyss");
    }
}

TEST(BiomeIsolation, LandmarksDontLeakBetweenBiomes) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    EXPECT_TRUE(load_landmark_defs("resources/landmarks.json"));
    auto prison = get_landmarks_for_biome("forgotten_prison");
    EXPECT_GE(prison.size(), 1u);
    for (auto* lm : prison) {
        EXPECT_NE(lm->icon, "≈") << lm->id << " has volcano icon";
    }
}

TEST(BiomeCoverage, AllFloorsMapped) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    for (int f=1; f<=15; f++)
        EXPECT_NE(get_biome_for_floor(f), nullptr) << "Floor " << f;
}

TEST(BiomeCoverage, ThreeBiomesDistinct) {
    EXPECT_TRUE(load_biome_defs("resources/biomes.json"));
    auto* a = get_biome_for_floor(1);
    auto* b = get_biome_for_floor(7);
    auto* c = get_biome_for_floor(14);
    EXPECT_NE(a->id, b->id);
    EXPECT_NE(b->id, c->id);
    EXPECT_NE(a->id, c->id);
}
