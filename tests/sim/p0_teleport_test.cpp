// P0-M2: SimAI teleport target regression tests
// Locks the room-domain contract that prevents the F1 permanent deadlock:
//   v1.2.3 room boundary (monster IDLE cross-room)
//   x Room Encounter LOCKED doors
//   x old teleport dropping player on corridor/door tiles (-1 room)
//   -> monster IDLE forever, player never reaches attack range, 900s timeout
#include <gtest/gtest.h>
#include "core/sim/sim_ai_teleport.h"
#include "world/game_map.h"
#include "world/room_manager.h"
#include "entities/monster.h"
#include <memory>

namespace {
// Build a 9x7 map: room A (x1..3), corridor col 4, room B (x5..7), y1..5
// door tile at (4,3) connecting them. Mirrors real dungeon topology.
struct TestWorld {
    std::shared_ptr<GameMap> map;
    RoomManager rooms;

    TestWorld() {
        map = std::make_shared<GameMap>(9, 7, 32);
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 9; x++)
                map->set_tile(x, y, TileType::WALL);
        // room A interior
        for (int y = 1; y <= 5; y++)
            for (int x = 1; x <= 3; x++)
                map->set_tile(x, y, TileType::FLOOR);
        // corridor
        for (int y = 1; y <= 5; y++)
            map->set_tile(4, y, TileType::FLOOR);
        // room B interior
        for (int y = 1; y <= 5; y++)
            for (int x = 5; x <= 7; x++)
                map->set_tile(x, y, TileType::FLOOR);
        // door on corridor middle (room B entrance)
        map->set_tile(4, 3, TileType::DOOR);
        std::vector<std::tuple<int,int,int,int>> room_rects = {
            {1, 1, 3, 5},   // room A
            {5, 1, 3, 5},   // room B
        };
        rooms.build(map.get(), room_rects, /*is_boss_floor=*/false);
    }
};

std::unique_ptr<Monster> make_monster(float px, float py) {
    return std::make_unique<Monster>(px, py, "test_orc", 30, 5, 0, 0,
                                     Color{200, 60, 60, 255}, nullptr);
}
} // namespace

// TEST 1: LockedEncounterTeleportSameRoom
// Teleport target must land the player in the SAME room domain as the
// target monster — never on corridor/door tiles (-1) when a same-room
// walkable tile exists.
TEST(P0M2Teleport, LockedEncounterTeleportSameRoom) {
    TestWorld w;
    // Monster deep in room B (tile 6,3); door LOCKED (player locked outside)
    auto m = make_monster(6 * 32.0f + 14, 3 * 32.0f + 14);
    w.map->set_door_state(4, 3, DoorState::LOCKED);

    TeleportQuery q;
    q.player_rect = {32.0f, 32.0f, 28.0f, 28.0f};   // in room A
    q.target = m.get();
    q.map = w.map.get();
    q.rooms = &w.rooms;

    TeleportResult r = sim_ai_teleport_target(q);
    ASSERT_TRUE(r.found);
    // Contract: same room as monster
    int room_of_target = w.rooms.room_at(6, 3);
    int room_of_landing = w.rooms.room_at(r.tile_x, r.tile_y);
    EXPECT_EQ(room_of_landing, room_of_target)
        << "teleport must not drop player into corridor (-1) while "
           "same-room tiles exist";
    EXPECT_NE(room_of_landing, -1);
    EXPECT_TRUE(w.map->is_walkable(r.tile_x, r.tile_y));
}

// TEST 2: TeleportAvoidsInvalidTarget
// Landing tile must be: walkable, not wall, not LOCKED/SEALED door,
// not overlapping another living monster.
TEST(P0M2Teleport, TeleportAvoidsInvalidTarget) {
    TestWorld w;
    // Target monster at (6,3); another monster physically blocks (7,3)
    auto target = make_monster(6 * 32.0f + 14, 3 * 32.0f + 14);
    auto blocker = make_monster(7 * 32.0f + 14, 3 * 32.0f + 14);
    w.map->set_door_state(4, 3, DoorState::LOCKED);

    TeleportQuery q;
    q.player_rect = {32.0f, 32.0f, 28.0f, 28.0f};
    q.target = target.get();
    q.map = w.map.get();
    q.rooms = &w.rooms;
    q.extra_monsters.push_back(blocker.get());

    TeleportResult r = sim_ai_teleport_target(q);
    ASSERT_TRUE(r.found);
    // Not the blocker tile (occupied)
    EXPECT_FALSE(r.tile_x == 7 && r.tile_y == 3)
        << "must not teleport onto an occupied monster tile";
    // Not a wall
    EXPECT_NE(w.map->tile_at(r.tile_x, r.tile_y), TileType::WALL);
    // Not LOCKED/SEALED door
    DoorState ds = w.map->door_state_at(r.tile_x, r.tile_y);
    EXPECT_TRUE(ds != DoorState::LOCKED && ds != DoorState::SEALED);
    EXPECT_TRUE(w.map->is_walkable(r.tile_x, r.tile_y));
}

// TEST 3: NoSameRoomTileFallbackStillWalkable
// If room B were fully occupied (no free same-room tile), fallback must
// still pick a walkable non-wall tile rather than fail silently into the
// old deadlock (or teleport into a wall).
TEST(P0M2Teleport, NoSameRoomTileFallbackStillWalkable) {
    TestWorld w;
    auto target = make_monster(6 * 32.0f + 14, 3 * 32.0f + 14);
    // Fill every room-B tile except the target's with blockers
    std::vector<std::unique_ptr<Monster>> blockers;
    for (int y = 1; y <= 5; y++)
        for (int x = 5; x <= 7; x++) {
            if (x == 6 && y == 3) continue;
            blockers.push_back(make_monster(x * 32.0f + 14, y * 32.0f + 14));
        }

    TeleportQuery q;
    q.player_rect = {32.0f, 32.0f, 28.0f, 28.0f};
    q.target = target.get();
    q.map = w.map.get();
    q.rooms = &w.rooms;
    for (auto& b : blockers) q.extra_monsters.push_back(b.get());

    TeleportResult r = sim_ai_teleport_target(q);
    // Either a valid fallback exists (walkable, not wall)...
    if (r.found) {
        EXPECT_NE(w.map->tile_at(r.tile_x, r.tile_y), TileType::WALL);
        EXPECT_TRUE(w.map->is_walkable(r.tile_x, r.tile_y));
    } else {
        // ...or honest failure (caller keeps old position; no corruption)
        SUCCEED();
    }
}
