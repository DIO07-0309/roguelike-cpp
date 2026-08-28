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

// ── Batch 1: Door 状态语义 ─────────────────────────────────

// OPEN 门 (默认态): 透视线 + 可行走 — 锁定 Phase 2 行为
TEST_F(FOVTest, DoorOpen_TransparentAndWalkable) {
    map->set_tile(6, 3, TileType::DOOR);            // (5,5) 是墙, y=3 水平线通畅
    EXPECT_TRUE(map->is_door(6, 3));
    EXPECT_EQ(map->door_state_at(6, 3), DoorState::OPEN);
    EXPECT_TRUE(map->is_walkable(6, 3));
    EXPECT_FALSE(map->blocks_sight(6, 3));
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(7, 3));              // 门后 tile 可见 (透射)
}

// CLOSED 门: 挡视线 — 门 tile 自身可见, 门后不可见
TEST_F(FOVTest, DoorClosed_BlocksSight) {
    map->set_tile(6, 3, TileType::DOOR);
    ASSERT_TRUE(map->set_door_state(6, 3, DoorState::CLOSED));
    EXPECT_TRUE(map->blocks_sight(6, 3));
    map->update_fov(3, 3, 5);
    EXPECT_TRUE(map->isVisible(6, 3));              // 门 tile 自身可见 (射线先标记后 break)
    EXPECT_TRUE(map->isExplored(6, 3));
    EXPECT_FALSE(map->isVisible(7, 3));             // 门后不可见
    EXPECT_FALSE(map->isExplored(7, 3));
}

// CLOSED 门: 不可行走; 非 door tile set_door_state 返回 false; 切回 OPEN 恢复
TEST_F(FOVTest, DoorClosed_NotWalkable_AndReopen) {
    map->set_tile(6, 3, TileType::DOOR);
    ASSERT_TRUE(map->set_door_state(6, 3, DoorState::CLOSED));
    EXPECT_FALSE(map->is_walkable(6, 3));
    EXPECT_FALSE(map->set_door_state(5, 5, DoorState::CLOSED));  // (5,5) 是 WALL → false
    EXPECT_EQ(map->door_state_at(5, 5), DoorState::NONE);
    EXPECT_FALSE(map->is_door(5, 5));
    EXPECT_TRUE(map->set_door_state(6, 3, DoorState::OPEN));     // 重新开启
    EXPECT_TRUE(map->is_walkable(6, 3));
    EXPECT_FALSE(map->blocks_sight(6, 3));
}
