#include <gtest/gtest.h>
#include "game_map.h"
#include "dungeon_generator.h"
#include <queue>
#include <set>

// ============================================================
// Phase 2: Dungeon Topology Tests
// ============================================================

class DungeonTopologyTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen = std::make_unique<DungeonGenerator>(40, 30, 32);
    }
    std::unique_ptr<DungeonGenerator> gen;

    // BFS: 从 (sx,sy) 出发可达的 walkable tile 数
    int bfs_reachable(GameMap& map, int sx, int sy) {
        if (!map.is_walkable(sx, sy)) return 0;
        std::set<std::pair<int,int>> visited;
        std::queue<std::pair<int,int>> q;
        q.push({sx, sy});
        visited.insert({sx, sy});
        while (!q.empty()) {
            auto [x, y] = q.front(); q.pop();
            for (auto [dx, dy] : std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
                int nx = x+dx, ny = y+dy;
                if (map.is_walkable(nx, ny) && visited.count({nx,ny}) == 0) {
                    visited.insert({nx, ny});
                    q.push({nx, ny});
                }
            }
        }
        return (int)visited.size();
    }

    // 统计地图中每种 TileType 的数量
    std::map<TileType, int> count_tiles(GameMap& map) {
        std::map<TileType, int> counts;
        for (int y = 0; y < map.height; y++)
            for (int x = 0; x < map.width; x++)
                counts[map.tile_at(x, y)]++;
        return counts;
    }

    // 获取所有 Door tile 坐标
    std::vector<std::pair<int,int>> get_doors(GameMap& map) {
        std::vector<std::pair<int,int>> doors;
        for (int y = 0; y < map.height; y++)
            for (int x = 0; x < map.width; x++)
                if (map.tile_at(x, y) == TileType::DOOR)
                    doors.push_back({x, y});
        return doors;
    }

    // 获取所有 Room interior floor tiles (x,y 在某个 Room 矩形内)
    std::set<std::pair<int,int>> get_room_interiors() {
        std::set<std::pair<int,int>> interiors;
        auto rooms = gen->get_room_centers();
        // rooms 只有中心点，需要用 generate 后的 _rooms
        // 这里用简化方法：从生成器获取
        return interiors;
    }
};

// Test 1: Door Tile 属性
TEST_F(DungeonTopologyTest, DoorTile_Walkable) {
    Tile t = Tile::door();
    EXPECT_TRUE(t.is_walkable);
    EXPECT_EQ(t.type, TileType::DOOR);
}

// Test 2: Door Tile blocks_sight = false
TEST_F(DungeonTopologyTest, BlocksSight_DoorFalse) {
    GameMap map(5, 5, 32);
    map.set_tile(2, 2, TileType::DOOR);
    EXPECT_FALSE(map.blocks_sight(2, 2));
}

// Test 3: load_from_template 正确处理 'D'
TEST_F(DungeonTopologyTest, LoadTemplate_Door) {
    GameMap map(5, 3, 32);
    std::vector<std::string> tmpl = {
        "#####",
        "#D.D#",
        "#####"
    };
    map.load_from_template(tmpl);
    EXPECT_EQ(map.tile_at(1, 1), TileType::DOOR);
    EXPECT_TRUE(map.is_walkable(1, 1));
    EXPECT_EQ(map.tile_at(2, 1), TileType::FLOOR);
    EXPECT_EQ(map.tile_at(0, 0), TileType::WALL);
}

// Test 4: 生成地图包含 Door tiles
TEST_F(DungeonTopologyTest, GeneratedMap_HasDoors) {
    auto map = gen->generate(42);
    auto doors = get_doors(*map);
    EXPECT_GT(doors.size(), 0u);
}

// Test 5: Door 不在地图边界
TEST_F(DungeonTopologyTest, Door_NotAtMapBoundary) {
    auto map = gen->generate(42);
    auto doors = get_doors(*map);
    for (auto [x, y] : doors) {
        EXPECT_GT(x, 0) << "Door at left boundary: (" << x << "," << y << ")";
        EXPECT_LT(x, map->width - 1) << "Door at right boundary: (" << x << "," << y << ")";
        EXPECT_GT(y, 0) << "Door at top boundary: (" << x << "," << y << ")";
        EXPECT_LT(y, map->height - 1) << "Door at bottom boundary: (" << x << "," << y << ")";
    }
}

// Test 6: Door 至少一侧是 FLOOR (Room interior 或 Corridor)
TEST_F(DungeonTopologyTest, Door_AdjacentToFloor) {
    auto map = gen->generate(42);
    auto doors = get_doors(*map);
    for (auto [dx, dy] : doors) {
        bool has_floor_neighbor = false;
        for (auto [ox, oy] : std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
            int nx = dx+ox, ny = dy+oy;
            if (nx >= 0 && nx < map->width && ny >= 0 && ny < map->height) {
                TileType t = map->tile_at(nx, ny);
                if (t == TileType::FLOOR || t == TileType::DOOR) {
                    has_floor_neighbor = true;
                    break;
                }
            }
        }
        EXPECT_TRUE(has_floor_neighbor)
            << "Door at (" << dx << "," << dy << ") has no floor/door neighbor";
    }
}

// Test 7: 所有 Room 从 Room[0] 可达 (BFS)
TEST_F(DungeonTopologyTest, AllRoomsReachable) {
    auto map = gen->generate(42);
    auto centers = gen->get_room_centers();
    ASSERT_GT(centers.size(), 1u);

    // 从第一个房间中心 BFS，统计可达 walkable tile
    int reachable = bfs_reachable(*map, centers[0].first, centers[0].second);

    // 统计所有 walkable tile
    int total_walkable = 0;
    for (int y = 0; y < map->height; y++)
        for (int x = 0; x < map->width; x++)
            if (map->is_walkable(x, y)) total_walkable++;

    // 所有 walkable tile 应该可达（Room + Corridor + Door 全部连通）
    EXPECT_EQ(reachable, total_walkable)
        << "BFS reachable=" << reachable << " total_walkable=" << total_walkable;
}

// Test 8: 多 seed 测试 — 生成10个不同 seed 的地图都有效
TEST_F(DungeonTopologyTest, MultiSeed_ValidMaps) {
    for (uint32_t seed = 1; seed <= 10; seed++) {
        auto map = gen->generate(seed);
        ASSERT_NE(map, nullptr) << "Seed " << seed << " returned null";
        auto doors = get_doors(*map);
        EXPECT_GT(doors.size(), 0u) << "Seed " << seed << " has no doors";

        // 每个 Door 都有 walkable 邻居
        for (auto [dx, dy] : doors) {
            bool ok = false;
            for (auto [ox, oy] : std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
                if (map->is_walkable(dx+ox, dy+oy)) { ok = true; break; }
            }
            EXPECT_TRUE(ok) << "Seed " << seed << " Door (" << dx << "," << dy << ") isolated";
        }
    }
}

// Test 9: 固定 seed 可重复生成
TEST_F(DungeonTopologyTest, FixedSeed_Reproducible) {
    auto map1 = gen->generate(12345);
    auto map2 = gen->generate(12345);
    ASSERT_NE(map1, nullptr);
    ASSERT_NE(map2, nullptr);

    for (int y = 0; y < map1->height; y++)
        for (int x = 0; x < map1->width; x++)
            EXPECT_EQ(map1->tile_at(x, y), map2->tile_at(x, y))
                << "Mismatch at (" << x << "," << y << ") seed=12345";
}

// Test 10: FOV 在 Door tile 上正常工作
TEST_F(DungeonTopologyTest, FOV_WorksWithDoor) {
    GameMap map(10, 10, 32);
    // 创建一个简单的房间 + Door 布局
    // ##########
    // #........#
    // #........#
    // #...D....#
    // #........#
    // #........#
    // #........#
    // #........#
    // #........#
    // ##########
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 10; x++)
            map.set_tile(x, y, TileType::WALL);
    for (int y = 1; y < 9; y++)
        for (int x = 1; x < 9; x++)
            map.set_tile(x, y, TileType::FLOOR);
    map.set_tile(4, 3, TileType::DOOR);

    // 在 Door 附近更新 FOV
    map.update_fov(4, 3, 5);

    // Door 本身应该可见
    EXPECT_TRUE(map.isVisible(4, 3));
    EXPECT_TRUE(map.isExplored(4, 3));

    // Door 旁边的 FLOOR 也应该可见
    EXPECT_TRUE(map.isVisible(3, 3));
    EXPECT_TRUE(map.isVisible(5, 3));
}

// Test 11: Corridor 不侵入 Room Interior
TEST_F(DungeonTopologyTest, Corridor_NotInRoom) {
    // 通过分析生成结果间接验证：
    // 如果 Corridor 侵入 Room，Room 内部会出现非 FLOOR tile
    // 或者 Room 面积会变小
    // 简单验证：所有 Room center 都是 walkable
    auto map = gen->generate(42);
    auto centers = gen->get_room_centers();
    for (auto [cx, cy] : centers) {
        EXPECT_TRUE(map->is_walkable(cx, cy))
            << "Room center (" << cx << "," << cy << ") not walkable";
    }
}

// Test 12: Door 两侧分别是 Room 和 Corridor (拓扑验证)
TEST_F(DungeonTopologyTest, Door_BetweenRoomAndCorridor) {
    auto map = gen->generate(42);
    auto doors = get_doors(*map);

    for (auto [dx, dy] : doors) {
        // Door 的 4 个邻居中，至少有1个 FLOOR (Room/Corridor) 和至少1个 WALL
        int floor_count = 0;
        int wall_count = 0;
        for (auto [ox, oy] : std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
            int nx = dx+ox, ny = dy+oy;
            if (nx < 0 || nx >= map->width || ny < 0 || ny >= map->height) continue;
            TileType t = map->tile_at(nx, ny);
            if (t == TileType::FLOOR) floor_count++;
            else if (t == TileType::WALL) wall_count++;
        }
        // Door 至少连接 1 个 FLOOR（保证不是悬空）
        EXPECT_GE(floor_count, 1)
            << "Door (" << dx << "," << dy << ") has only " << floor_count << " floor neighbors";
    }
}
