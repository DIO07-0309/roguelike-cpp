#include <gtest/gtest.h>
#include "game_map.h"

class FOVTest : public ::testing::Test {
protected:
    void SetUp() override {
        map = std::make_unique<GameMap>(10, 10, 32);
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++)
                map->set_tile(x, y, TileType::FLOOR);
        map->set_tile(5, 5, TileType::WALL);
    }
    std::unique_ptr<GameMap> map;
};

TEST_F(FOVTest, PlayerTileVisibleAndExplored) {
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(3, 3));
    EXPECT_TRUE(map->isExplored(3, 3));
}

TEST_F(FOVTest, VisibleWithinRadius) {
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(4, 3));
    EXPECT_TRUE(map->isVisible(3, 4));
    EXPECT_TRUE(map->isVisible(2, 3));
}

TEST_F(FOVTest, BlockedByWall) {
    map->update_fov(3, 3, 5);
    EXPECT_FALSE(map->isVisible(6, 6));
}

TEST_F(FOVTest, WallItselfVisible) {
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(5, 5));
    EXPECT_TRUE(map->isExplored(5, 5));
}

TEST_F(FOVTest, LeavesTileBecomesExplored) {
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(3, 3));

    map->update_fov(8, 8, 5);
    EXPECT_FALSE(map->isVisible(3, 3));
    EXPECT_TRUE(map->isExplored(3, 3));
}

TEST_F(FOVTest, ResetClearsExploration) {
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isExplored(3, 3));

    map->reset_visibility();
    EXPECT_FALSE(map->isVisible(3, 3));
    EXPECT_FALSE(map->isExplored(3, 3));
}

TEST_F(FOVTest, BlocksSightIndependentOfWalkable) {
    map->set_tile(7, 7, TileType::LAVA);
    EXPECT_TRUE(map->is_walkable(7, 7));
    EXPECT_FALSE(map->blocks_sight(7, 7));

    EXPECT_TRUE(map->blocks_sight(5, 5));
    EXPECT_FALSE(map->is_walkable(5, 5));
}

TEST_F(FOVTest, OutOfBoundsBlocksSight) {
    EXPECT_TRUE(map->blocks_sight(-1, 0));
    EXPECT_TRUE(map->blocks_sight(10, 5));
}
