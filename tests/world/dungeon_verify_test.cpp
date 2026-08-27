#include <gtest/gtest.h>
#include "dungeon_generator.h"
#include "game_map.h"
#include <set>
#include <deque>
#include <iostream>

// ============================================================
// Phase 2 实机验收 — 结构化拓扑验证 (P2-1..P2-5)
// ============================================================

struct DoorInfo {
    std::pair<int,int> door;
    std::pair<int,int> room_edge;   // 房间内部地板
    std::pair<int,int> corridor_side; // 走廊侧邻居 tile（可能是门或地板）
};

// 统计 walkable 连通分量（忽略墙/熔岩）
static std::set<std::pair<int,int>> flood(GameMap& m, int sx, int sy) {
    std::set<std::pair<int,int>> vis;
    if (!m.is_walkable(sx, sy)) return vis;
    std::deque<std::pair<int,int>> q{{sx, sy}};
    vis.insert({sx, sy});
    while (!q.empty()) {
        auto [x, y] = q.front(); q.pop_front();
        for (auto [dx,dy] : std::vector<std::pair<int,int>>{{1,0},{-1,0},{0,1},{0,-1}}) {
            int nx=x+dx, ny=y+dy;
            if (nx<0||nx>=m.width||ny<0||ny>=m.height) continue;
            if (!m.is_walkable(nx,ny)) continue;
            if (vis.count({nx,ny})) continue;
            vis.insert({nx,ny}); q.push_back({nx,ny});
        }
    }
    return vis;
}

// 检查 tile 是否是门或地板（可走且非墙）
static bool is_open(GameMap& m, int x, int y) {
    if (x<0||x>=m.width||y<0||y>=m.height) return false;
    auto t = m.tile_at(x,y);
    return t == TileType::FLOOR || t == TileType::DOOR || t == TileType::STAIRS_DOWN;
}

TEST(DungeonVerify, AllRoomsReachable_And_Connected) {
    const uint32_t seeds[] = {1,2,3,42,100,999,20240801};
    for (uint32_t seed : seeds) {
        DungeonGenerator gen(40, 30, 32);
        auto map = gen.generate(seed);

        // 1. 收集所有房间矩形
        auto rooms = gen.get_room_rects();
        ASSERT_GT(rooms.size(), 1u) << "seed=" << seed;

        // 2. 从第一个房间中心 flood
        auto [rx,ry,rw,rh] = rooms[0];
        int cx = rx+rw/2, cy = ry+rh/2;
        auto reachable = flood(*map, cx, cy);

        // 3. 每个房间的中心必须可达
        for (auto& [a,b,c,d] : rooms) {
            EXPECT_TRUE(reachable.count({a+c/2, b+d/2}))
                << "seed=" << seed << " room center (" << (a+c/2) << "," << (b+d/2)
                << ") NOT reachable";
        }
        std::cout << "[seed " << seed << "] rooms=" << rooms.size()
                  << " reachable_rooms=OK\n";
    }
}

TEST(DungeonVerify, Door_Count_Equals_Connections) {
    const uint32_t seeds[] = {1,2,3,42,100,999,20240801};
    for (uint32_t seed : seeds) {
        DungeonGenerator gen(40, 30, 32);
        auto map = gen.generate(seed);
        auto conns = gen.get_connections();

        // 统计实际放置的 Door tile 数量
        int door_tiles = 0;
        for (int y=0;y<map->height;y++)
            for (int x=0;x<map->width;x++)
                if (map->tile_at(x,y)==TileType::DOOR) door_tiles++;

        // 期望：每次连接放 2 扇门，但两扇门可能坐标重合
        std::set<std::pair<int,int>> expected_doors;
        for (auto& c : conns) { expected_doors.insert(c.door_a); expected_doors.insert(c.door_b); }

        EXPECT_EQ(door_tiles, (int)expected_doors.size())
            << "seed=" << seed << " door_tiles=" << door_tiles
            << " expected_unique=" << expected_doors.size()
            << " connections=" << conns.size();
        std::cout << "[seed " << seed << "] doors_placed=" << door_tiles
                  << " connections=" << conns.size() << "\n";
    }
}

TEST(DungeonVerify, Door_NoFloating_NoBoundary) {
    const uint32_t seeds[] = {1,2,3,42,100,999,20240801};
    for (uint32_t seed : seeds) {
        DungeonGenerator gen(40, 30, 32);
        auto map = gen.generate(seed);
        int door_tiles = 0;
        for (int y=0;y<map->height;y++)
            for (int x=0;x<map->width;x++) {
                if (map->tile_at(x,y)!=TileType::DOOR) continue;
                door_tiles++;
                // 越界检查
                EXPECT_TRUE(x>0 && x<map->width-1 && y>0 && y<map->height-1)
                    << "seed=" << seed << " Door boundary (" << x << "," << y << ")";
                // 非悬空：至少 2 个 open 邻居（一边房间一边走廊）
                int open_n = (is_open(*map,x+1,y)?1:0)+(is_open(*map,x-1,y)?1:0)
                            +(is_open(*map,x,y+1)?1:0)+(is_open(*map,x,y-1)?1:0);
                EXPECT_GE(open_n, 2)
                    << "seed=" << seed << " Door (" << x << "," << y
                    << ") has " << open_n << " open neighbors";
            }
        std::cout << "[seed " << seed << "] doors_checked=" << door_tiles << " (no float/boundary)\n";
    }
}

// P2-2: 走廊不侵入 Room Interior
TEST(DungeonVerify, Rooms_Intact_AfterCorridor) {
    const uint32_t seeds[] = {1,2,3,42,100,999,20240801};
    for (uint32_t seed : seeds) {
        DungeonGenerator gen(40, 30, 32);
        auto map = gen.generate(seed);
        auto rooms = gen.get_room_rects();

        int room_center_mismatch = 0, room_corner_mismatch = 0;
        for (auto& [rx,ry,rw,rh] : rooms) {
            // 房间中心必须是 FLOOR（不是走廊侵入导致）
            auto t_center = map->tile_at(rx+rw/2, ry+rh/2);
            if (t_center != TileType::FLOOR) room_center_mismatch++;
            // 房间四个角必须是可走（不被墙/tile 覆盖）
            if (!map->is_walkable(rx,ry)) room_corner_mismatch++;
            if (!map->is_walkable(rx+rw-1,ry)) room_corner_mismatch++;
            if (!map->is_walkable(rx,ry+rh-1)) room_corner_mismatch++;
            if (!map->is_walkable(rx+rw-1,ry+rh-1)) room_corner_mismatch++;
        }
        EXPECT_EQ(room_center_mismatch, 0) << "seed=" << seed << " rooms corrupted by corridor";
        std::cout << "[seed " << seed << "] rooms_intact center_bad=" << room_center_mismatch
                  << " corner_bad=" << room_corner_mismatch << "\n";
    }
}

TEST(DungeonVerify, Door_ConnectsRoomAndCorridor) {
    const uint32_t seeds[] = {1,2,3,42,100,999,20240801};
    for (uint32_t seed : seeds) {
        DungeonGenerator gen(40, 30, 32);
        auto map = gen.generate(seed);
        auto conns = gen.get_connections();
        auto rooms = gen.get_room_rects();

        // 构建房间集合（所有房间内部 floor 坐标）
        std::set<std::pair<int,int>> room_interior;
        for (auto& [rx,ry,rw,rh] : rooms)
            for (int y=ry;y<ry+rh;y++)
                for (int x=rx;x<rx+rw;x++)
                    room_interior.insert({x,y});

        for (auto& c : conns) {
            // door_a 应至少有一个邻居在 room_interior 中
            bool a_room = room_interior.count({c.door_a.first+1,c.door_a.second})
                       || room_interior.count({c.door_a.first-1,c.door_a.second})
                       || room_interior.count({c.door_a.first,c.door_a.second+1})
                       || room_interior.count({c.door_a.first,c.door_a.second-1});
            bool b_room = room_interior.count({c.door_b.first+1,c.door_b.second})
                       || room_interior.count({c.door_b.first-1,c.door_b.second})
                       || room_interior.count({c.door_b.first,c.door_b.second+1})
                       || room_interior.count({c.door_b.first,c.door_b.second-1});
            EXPECT_TRUE(a_room) << "seed=" << seed << " door_a (" << c.door_a.first
                                << "," << c.door_a.second << ") not adjacent to room";
            EXPECT_TRUE(b_room) << "seed=" << seed << " door_b (" << c.door_b.first
                                << "," << c.door_b.second << ") not adjacent to room";
        }
        std::cout << "[seed " << seed << "] connections_valid=" << conns.size() << "\n";
    }
}
