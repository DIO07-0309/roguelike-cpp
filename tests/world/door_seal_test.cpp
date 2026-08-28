#include <gtest/gtest.h>
#include "game_map.h"
#include "dungeon_generator.h"
#include "config.h"
#include <queue>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <utility>

// ============================================================
// Batch 1: Door Seal & Aperture Integrity 
// INVARIANT(seal): DOOR ?
//   -  DOOR , ?BFS,  tile 
//   - ?FLOOR  (Non-Door Gap == 0)
//   - 
//   -  (>=1 open  / )
//   - DoorState  (OPEN/CLOSED ?is_walkable / blocks_sight)
// ============================================================

namespace {

struct RoomR { int rx, ry, rw, rh; };

bool in_room(const RoomR& r, int x, int y) {
    return x >= r.rx && x < r.rx + r.rw && y >= r.ry && y < r.ry + r.rh;
}

int total_walkable(GameMap& m) {
    int n = 0;
    for (int y = 0; y < m.height; y++)
        for (int x = 0; x < m.width; x++)
            if (m.is_walkable(x, y)) n++;
    return n;
}

}  // namespace

class DoorSealTest : public ::testing::Test {
protected:
    void SetUp() override {
        gen = std::make_unique<DungeonGenerator>(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    }
    std::unique_ptr<DungeonGenerator> gen;
};

// ?seed : 7  (? + 20 fuzz
static const uint32_t kSeeds[] = {
    1,2,3,42,100,999,20240801,
    1000,1001,1002,1003,1004,1005,1006,1007,1008,1009,
    1010,1011,1012,1013,1014,1015,1016,1017,1018,1019
};

// T1: INVARIANT(seal) ?, ?BFS,  tile 
TEST_F(DoorSealTest, Invariant_Seal_AllSeeds) {
    for (uint32_t seed : kSeeds) {
        auto map = gen->generate(seed);
        std::vector<RoomR> rooms;
        for (auto& t : gen->get_room_rects()) {
            rooms.push_back({std::get<0>(t), std::get<1>(t), std::get<2>(t), std::get<3>(t)});
        }
        ASSERT_FALSE(rooms.empty()) << "seed=" << seed;

        // ?(walkable ?DOOR ?
        int sx = -1, sy = -1;
        for (int y = 0; y < MAP_HEIGHT && sx < 0; y++)
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (!map->is_walkable(x, y) || map->tile_at(x, y) == TileType::DOOR) continue;
                bool interior = false;
                for (auto& r : rooms)
                    if (in_room(r, x, y)) { interior = true; break; }
                if (!interior) { sx = x; sy = y; break; }
            }
        ASSERT_GE(sx, 0) << "seed=" << seed << " ";

        std::set<std::pair<int,int>> reach;
        {
            std::queue<std::pair<int,int>> q;
            q.push({sx, sy}); reach.insert({sx, sy});
            const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
            while (!q.empty()) {
                auto c = q.front(); q.pop();
                for (int d = 0; d < 4; d++) {
                    int nx = c.first + DX[d], ny = c.second + DY[d];
                    if (nx < 0 || ny < 0 || nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;
                    if (!map->is_walkable(nx, ny)) continue;             // DOOR walkable 
                    if (map->tile_at(nx, ny) == TileType::DOOR) continue; // ?
                    if (reach.count({nx, ny})) continue;
                    reach.insert({nx, ny});
                    q.push({nx, ny});
                }
            }
        }
        for (auto& r : rooms) {
            for (int y = r.ry; y < r.ry + r.rh; y++)
                for (int x = r.rx; x < r.rx + r.rw; x++)
                    EXPECT_FALSE(reach.count({x, y}))
                        << "seed=" << seed << "  (" << r.rx << "," << r.ry
                        << ")  (" << x << "," << y << ") ";
        }
    }
}

// T2: Non-Door Gap == 0 -- ?WALL ?DOOR
TEST_F(DoorSealTest, NoNonDoorGap_AllSeeds) {
    for (uint32_t seed : kSeeds) {
        auto map = gen->generate(seed);
        for (auto& t : gen->get_room_rects()) {
            int rx = std::get<0>(t), ry = std::get<1>(t), rw = std::get<2>(t), rh = std::get<3>(t);
            for (int x = rx; x < rx + rw; x++) {
                if (ry - 1 >= 0) {
                    TileType tt = map->tile_at(x, ry - 1);
                    EXPECT_TRUE(tt == TileType::WALL || tt == TileType::DOOR)
                        << "seed=" << seed << "  (" << x << "," << ry-1 << ") type=" << (int)tt;
                }
                if (ry + rh < MAP_HEIGHT) {
                    TileType tt = map->tile_at(x, ry + rh);
                    EXPECT_TRUE(tt == TileType::WALL || tt == TileType::DOOR)
                        << "seed=" << seed << "  (" << x << "," << ry+rh << ") type=" << (int)tt;
                }
            }
            for (int y = ry; y < ry + rh; y++) {
                if (rx - 1 >= 0) {
                    TileType tt = map->tile_at(rx - 1, y);
                    EXPECT_TRUE(tt == TileType::WALL || tt == TileType::DOOR)
                        << "seed=" << seed << "  (" << rx-1 << "," << y << ") type=" << (int)tt;
                }
                if (rx + rw < MAP_WIDTH) {
                    TileType tt = map->tile_at(rx + rw, y);
                    EXPECT_TRUE(tt == TileType::WALL || tt == TileType::DOOR)
                        << "seed=" << seed << "  (" << rx+rw << "," << y << ") type=" << (int)tt;
                }
            }
        }
    }
}

// T3: 
TEST_F(DoorSealTest, FullyConnected_NoDeadRoom_AllSeeds) {
    for (uint32_t seed : kSeeds) {
        auto map = gen->generate(seed);
        auto rooms = gen->get_room_rects();
        auto [rx, ry, rw, rh] = rooms[0];
        int cx = rx + rw/2, cy = ry + rh/2;
        std::set<std::pair<int,int>> vis;
        std::queue<std::pair<int,int>> q;
        q.push({cx, cy}); vis.insert({cx, cy});
        const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
        while (!q.empty()) {
            auto c = q.front(); q.pop();
            for (int d = 0; d < 4; d++) {
                int nx = c.first + DX[d], ny = c.second + DY[d];
                if (nx < 0 || ny < 0 || nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;
                if (!map->is_walkable(nx, ny)) continue;
                if (vis.count({nx, ny})) continue;
                vis.insert({nx, ny});
                q.push({nx, ny});
            }
        }
        EXPECT_EQ((int)vis.size(), total_walkable(*map))
            << "seed=" << seed << " ?( " << vis.size()
            << "/" << total_walkable(*map) << ")";
    }
}

// T4: Door not floating (>=1 open neighbor) and in-bounds
TEST_F(DoorSealTest, DoorNoFloating_AllSeeds) {
    const int DX[4] = {1,-1,0,0}, DY[4] = {0,0,1,-1};
    for (uint32_t seed : kSeeds) {
        auto map = gen->generate(seed);
        for (int y = 0; y < MAP_HEIGHT; y++)
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (map->tile_at(x, y) != TileType::DOOR) continue;
                EXPECT_TRUE(x > 0 && x < MAP_WIDTH-1 && y > 0 && y < MAP_HEIGHT-1)
                    << "seed=" << seed << " Door ?(" << x << "," << y << ")";
                int open_n = 0;
                for (int d = 0; d < 4; d++) {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx < 0 || ny < 0 || nx >= MAP_WIDTH || ny >= MAP_HEIGHT) continue;
                    if (map->is_walkable(nx, ny)) open_n++;
                }
                EXPECT_GE(open_n, 1)
                    << "seed=" << seed << " Door (" << x << "," << y << ")  (0 open )";
            }
    }
}

// T5: DoorState  -- OPEN/CLOSED ?is_walkable / blocks_sight;  tile 
TEST_F(DoorSealTest, DoorState_Semantics) {
    GameMap m(6, 6, 32);
    m.set_tile(3, 3, TileType::DOOR);
    EXPECT_EQ(m.door_state_at(3, 3), DoorState::OPEN);
    EXPECT_TRUE(m.is_walkable(3, 3));
    EXPECT_FALSE(m.blocks_sight(3, 3));

    EXPECT_TRUE(m.set_door_state(3, 3, DoorState::CLOSED));
    EXPECT_EQ(m.door_state_at(3, 3), DoorState::CLOSED);
    EXPECT_FALSE(m.is_walkable(3, 3));
    EXPECT_TRUE(m.blocks_sight(3, 3));

    EXPECT_FALSE(m.set_door_state(0, 0, DoorState::CLOSED));   // WALL
    EXPECT_EQ(m.door_state_at(0, 0), DoorState::NONE);
    m.set_tile(1, 1, TileType::FLOOR);
    EXPECT_FALSE(m.set_door_state(1, 1, DoorState::CLOSED));   // FLOOR
    EXPECT_EQ(m.door_state_at(1, 1), DoorState::NONE);

    EXPECT_TRUE(m.set_door_state(3, 3, DoorState::OPEN));      // 
    EXPECT_TRUE(m.is_walkable(3, 3));
    EXPECT_FALSE(m.blocks_sight(3, 3));
}

// ? DOOR tile => is_walkable == (door_state == OPEN) 
TEST_F(DoorSealTest, DoorState_Consistency_AllSeeds) {
    for (uint32_t seed : kSeeds) {
        auto map = gen->generate(seed);
        for (int y = 0; y < MAP_HEIGHT; y++)
            for (int x = 0; x < MAP_WIDTH; x++)
                if (map->tile_at(x, y) == TileType::DOOR)
                    EXPECT_EQ(map->is_walkable(x, y), map->door_state_at(x, y) == DoorState::OPEN)
                        << "seed=" << seed << " Door (" << x << "," << y << ") ";
    }
}

