#include <gtest/gtest.h>
#include "minimap.h"
#include "game_map.h"
#include "config.h"

// ============================================================
// Phase 3: Minimap 单元测试 — 只测纯函数，不依赖 Raylib 绘制
// ============================================================

class MinimapTest : public ::testing::Test {
protected:
    void SetUp() override {
        map = std::make_unique<GameMap>(10, 10, 32);
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++)
                map->set_tile(x, y, TileType::FLOOR);
    }
    std::unique_ptr<GameMap> map;
};

// ---- tile_to_screen: 坐标换算 ----
TEST_F(MinimapTest, TileToScreen_Origin) {
    Rectangle panel = {100, 200, 160, 120};
    Rectangle r = MinimapRenderer::tile_to_screen(0, 0, panel);
    EXPECT_FLOAT_EQ(r.x, 100);
    EXPECT_FLOAT_EQ(r.y, 200);
    EXPECT_FLOAT_EQ(r.width, MINIMAP_TILE_SIZE);
}

TEST_F(MinimapTest, TileToScreen_Offset) {
    Rectangle panel = {100, 200, 160, 120};
    Rectangle r = MinimapRenderer::tile_to_screen(3, 5, panel);
    EXPECT_FLOAT_EQ(r.x, 100 + 3 * MINIMAP_TILE_SIZE);
    EXPECT_FLOAT_EQ(r.y, 200 + 5 * MINIMAP_TILE_SIZE);
}

TEST_F(MinimapTest, TileToScreen_FullMapSize) {
    // 40x30 地图 → 面板应容纳 40*4=160 宽, 30*4=120 高
    Rectangle panel = {0, 0, MINIMAP_WIDTH, MINIMAP_HEIGHT};
    Rectangle last = MinimapRenderer::tile_to_screen(MAP_WIDTH - 1, MAP_HEIGHT - 1, panel);
    EXPECT_FLOAT_EQ(last.x + MINIMAP_TILE_SIZE, MINIMAP_WIDTH);
    EXPECT_FLOAT_EQ(last.y + MINIMAP_TILE_SIZE, MINIMAP_HEIGHT);
}

// ---- color_for_tile: 类型 + 可见性 ----
TEST_F(MinimapTest, ColorForFloor) {
    Color c = MinimapRenderer::color_for_tile(TileType::FLOOR, true);
    EXPECT_TRUE(c.r > 0 || c.g > 0 || c.b > 0);  // 非透明
}

TEST_F(MinimapTest, Color_VisibleVsNotVisible) {
    Color vis = MinimapRenderer::color_for_tile(TileType::WALL, true);
    Color notvis = MinimapRenderer::color_for_tile(TileType::WALL, false);
    EXPECT_GT(vis.r, notvis.r);  // 当前可见比记忆态亮
    EXPECT_GT(vis.g, notvis.g);
    EXPECT_GT(vis.b, notvis.b);
}

TEST_F(MinimapTest, Color_DoorDistinctFromFloor) {
    Color door = MinimapRenderer::color_for_tile(TileType::DOOR, true);
    Color floor = MinimapRenderer::color_for_tile(TileType::FLOOR, true);
    EXPECT_NE(door.r, floor.r);  // Door 应有可辨识差异
}

// ---- should_draw_tile: 未探索不绘制 ----
TEST_F(MinimapTest, UnexploredTile_NotDrawn) {
    EXPECT_FALSE(MinimapRenderer::should_draw_tile(*map, 1, 1));  // 未探索
    map->update_fov(5, 5, 3);  // 探索 5,5 附近
    EXPECT_TRUE(MinimapRenderer::should_draw_tile(*map, 5, 5));
    EXPECT_FALSE(MinimapRenderer::should_draw_tile(*map, 0, 9));  // 远处未探索
}

// ---- 不泄露逻辑 ----
TEST_F(MinimapTest, Entity_OnlyVisible) {
    map->update_fov(5, 5, 3);
    EXPECT_TRUE(MinimapRenderer::should_show_entity(*map, 5, 5));   // 当前可见
    EXPECT_FALSE(MinimapRenderer::should_show_entity(*map, 0, 0));  // 未探索+不可见
}

TEST_F(MinimapTest, Entity_DissapearsWhenNotVisible) {
    map->update_fov(5, 5, 3);
    EXPECT_TRUE(MinimapRenderer::should_show_entity(*map, 5, 5));
    map->update_fov(0, 0, 3);  // 移动到远处
    EXPECT_FALSE(MinimapRenderer::should_show_entity(*map, 5, 5));  // 离开视野立即消失
}

TEST_F(MinimapTest, Boss_LastKnown_RequiresExplored) {
    // Boss 在未探索区域 → 永不显示 (不泄露)
    EXPECT_FALSE(MinimapRenderer::should_show_boss(*map, 8, 8));
    map->update_fov(8, 8, 3);  // 发现 Boss 位置
    EXPECT_TRUE(MinimapRenderer::should_show_boss(*map, 8, 8));  // 已探索 → 显示最后已知位置
}

TEST_F(MinimapTest, Stairs_ShownOnlyAfterDiscovered) {
    EXPECT_FALSE(MinimapRenderer::should_show_stairs(*map, 3, 3));
    map->update_fov(3, 3, 3);  // 发现楼梯
    EXPECT_TRUE(MinimapRenderer::should_show_stairs(*map, 3, 3));
}

TEST_F(MinimapTest, Boss_NegativeCoordinate_NotShown) {
    // 若无 Boss (_boss_last_known=-1)，应安全返回 false
    EXPECT_FALSE(MinimapRenderer::should_show_boss(*map, -1, -1));
}
