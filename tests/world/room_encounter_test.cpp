#include <gtest/gtest.h>
#include "room_manager.h"
#include "game_map.h"
#include "dungeon_generator.h"
#include "config.h"
#include "monster.h"
#include "player.h"
#include <memory>
#include <tuple>

// ============================================================
// Batch 2C: Room Encounter 单元测试
//   T1 触发+封门+清房完整闭环
//   T2 无怪房不触发
//   T3 玩家在门 tile 不落锁 (E1)
//   T4 Boss 房跳过
//   T5 多门房原子关闭/开启 (E3)
//   T6 build 门映射完整 (集成)
// ============================================================

namespace {

struct TestDungeon {
    std::unique_ptr<GameMap> map;
    std::vector<std::tuple<int,int,int,int>> rooms;
};

TestDungeon make_dungeon(int rx, int ry, int rw, int rh,
                         const std::vector<std::pair<int,int>>& door_positions) {
    int W = rx + rw + 3, H = ry + rh + 3;
    TestDungeon d;
    d.map = std::make_unique<GameMap>(W, H, 32);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            d.map->set_tile(x, y, TileType::WALL);
    for (int y = ry; y < ry + rh; y++)
        for (int x = rx; x < rx + rw; x++)
            d.map->set_tile(x, y, TileType::FLOOR);
    for (auto& [dx, dy] : door_positions) {
        d.map->set_tile(dx, dy, TileType::DOOR);
        if (dx == rx - 1) d.map->set_tile(dx - 1, dy, TileType::FLOOR);
        if (dx == rx + rw) d.map->set_tile(dx + 1, dy, TileType::FLOOR);
        if (dy == ry - 1) d.map->set_tile(dx, dy - 1, TileType::FLOOR);
        if (dy == ry + rh) d.map->set_tile(dx, dy + 1, TileType::FLOOR);
    }
    d.rooms.push_back({rx, ry, rw, rh});
    return d;
}

std::unique_ptr<Monster> make_monster(float px, float py) {
    auto m = std::make_unique<Monster>(px, py, "test_monster", 100, 10, 5, 5,
                                       Color{200, 80, 80, 255});
    m->entity.sync_rect();
    return m;
}

// 测试用 Player 构造 (x,y,speed,hp,atk,pdef,mdef)
Player make_player() {
    return Player(0.0f, 0.0f, 200.0f, 120, 12, 4, 2);
}

struct CallbackRecorder {
    int locked_count = 0;
    int cleared_count = 0;
    std::vector<int> locked_rooms;
    RoomEncounterCallbacks make_callbacks() {
        RoomEncounterCallbacks cb;
        cb.on_locked = [this](int idx) { locked_count++; locked_rooms.push_back(idx); };
        cb.on_cleared = [this](int) { cleared_count++; };
        return cb;
    }
};

void place_player(Player& p, int tx, int ty) {
    p.entity.position = {(float)(tx * 32), (float)(ty * 32)};
    p.entity.sync_rect();
}

}  // namespace

// T1: 完整闭环 进有怪房→封门→清房→开门
TEST(RoomEncounter, Trigger_Lock_And_Clear) {
    auto d = make_dungeon(2, 2, 5, 5, {{1, 4}});
    RoomManager mgr;
    CallbackRecorder rec;
    mgr.set_callbacks(rec.make_callbacks());
    mgr.build(d.map.get(), d.rooms, false);

    auto player = make_player();
    std::vector<std::unique_ptr<Monster>> monsters;
    monsters.push_back(make_monster(4 * 32 + 16, 4 * 32 + 16));

    place_player(player, 0, 4);   // 走廊
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_FALSE(mgr.is_room_locked(0));

    place_player(player, 3, 4);   // 进房
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_TRUE(mgr.is_room_locked(0));
    EXPECT_EQ(rec.locked_count, 1);
    EXPECT_FALSE(d.map->is_walkable(1, 4));

    monsters[0]->combat.is_alive = false;   // 击杀
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_FALSE(mgr.is_room_locked(0));
    EXPECT_TRUE(d.map->is_walkable(1, 4));
    EXPECT_EQ(rec.cleared_count, 1);
}

// T2: 无怪房不触发
TEST(RoomEncounter, NoMonster_RoomStaysIdle) {
    auto d = make_dungeon(2, 2, 5, 5, {{1, 4}});
    RoomManager mgr;
    mgr.set_callbacks(CallbackRecorder{}.make_callbacks());
    mgr.build(d.map.get(), d.rooms, false);

    auto player = make_player();
    std::vector<std::unique_ptr<Monster>> monsters;
    place_player(player, 3, 4);
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_FALSE(mgr.is_room_locked(0));
    EXPECT_TRUE(d.map->is_walkable(1, 4));
}

// T3: E1 边界 — 玩家在门 tile 不触发
TEST(RoomEncounter, PlayerOnDoor_NoLock) {
    auto d = make_dungeon(2, 2, 5, 5, {{1, 4}});
    RoomManager mgr;
    mgr.set_callbacks(CallbackRecorder{}.make_callbacks());
    mgr.build(d.map.get(), d.rooms, false);

    auto player = make_player();
    std::vector<std::unique_ptr<Monster>> monsters;
    monsters.push_back(make_monster(4 * 32 + 16, 4 * 32 + 16));
    // 玩家 center tile = (1,4) 在房间外 (房间从 x=2 起) -> room_at=-1 -> 不触发
    place_player(player, 1, 4);
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_FALSE(mgr.is_room_locked(0));
    EXPECT_TRUE(d.map->is_walkable(1, 4));
}

// T4: Boss 房跳过
TEST(RoomEncounter, BossRoom_Skip) {
    auto d = make_dungeon(2, 2, 5, 5, {{1, 4}});
    RoomManager mgr;
    mgr.set_callbacks(CallbackRecorder{}.make_callbacks());
    mgr.build(d.map.get(), d.rooms, true);   // is_boss_floor=true

    auto player = make_player();
    std::vector<std::unique_ptr<Monster>> monsters;
    monsters.push_back(make_monster(4 * 32 + 16, 4 * 32 + 16));
    place_player(player, 3, 4);
    mgr.tick(d.map.get(), &player, monsters);
    EXPECT_FALSE(mgr.is_room_locked(0));   // Boss 房不锁
    EXPECT_TRUE(d.map->is_walkable(1, 4));
}

// T5: 多门房原子关闭/开启 (E3)
TEST(RoomEncounter, MultiDoor_AtomicLock) {
    auto d = make_dungeon(2, 2, 5, 5, {{1, 4}, {7, 4}});  // 左右两门
    std::vector<std::pair<int,int>> doors = {{1, 4}, {7, 4}};
    EXPECT_TRUE(d.map->close_room_doors(doors));
    EXPECT_FALSE(d.map->is_walkable(1, 4));
    EXPECT_FALSE(d.map->is_walkable(7, 4));
    EXPECT_TRUE(d.map->open_room_doors(doors));
    EXPECT_TRUE(d.map->is_walkable(1, 4));
    EXPECT_TRUE(d.map->is_walkable(7, 4));
}

// T6: build 门映射完整 (生成地图)
TEST(RoomEncounter, Build_DoorMapping_OnGeneratedMap) {
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    auto map = gen.generate(42, 3, 0);
    auto rooms = gen.get_room_rects();
    int door_count = 0;
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            if (map->tile_at(x, y) == TileType::DOOR) door_count++;

    RoomManager mgr;
    mgr.set_callbacks(CallbackRecorder{}.make_callbacks());
    mgr.build(map.get(), rooms, false);

    for (int idx = 0; idx < (int)rooms.size(); idx++) {
        auto& r = rooms[idx];
        int cx = std::get<0>(r) + std::get<2>(r) / 2;
        int cy = std::get<1>(r) + std::get<3>(r) / 2;
        EXPECT_EQ(mgr.room_at(cx, cy), idx);
    }
    EXPECT_GT(door_count, 0);
}
