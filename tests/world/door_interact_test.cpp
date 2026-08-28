#include <gtest/gtest.h>
#include "game_map.h"
#include "dungeon_generator.h"
#include "config.h"
#include <queue>
#include <set>

// ============================================================
// Batch 2B: Door Interaction (R1 contact-open) 单元测试
//   - try_open_door_toward: 接触开门语义 (4 方向)
//   - CLOSED door 阻挡移动; OPEN 后可通行
//   - 非 door / 非 CLOSED tile 不触发开门
//   - 生成地图默认 OPEN (Batch 2B 不改变 gameplay 行为)
// ============================================================

namespace {

// 构造: 5x5, 中央为 DOOR, 左侧为 FLOOR, 右侧为 FLOOR
// 玩家 rect 用 32x32 (1 tile)
void build_door_map(GameMap& m, int door_x, int door_y, DoorState st) {
    m.set_tile(door_x, door_y, TileType::DOOR);
    ASSERT_TRUE(m.set_door_state(door_x, door_y, st));
}

}  // namespace

// 向右移动被 CLOSED 门阻挡 → 接触开门 → 门变 OPEN 可通行
TEST(DoorInteract, Right_ContactOpen_ClosedDoor) {
    GameMap m(6, 3, 32);
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 6; x++)
            m.set_tile(x, y, TileType::FLOOR);
    m.set_tile(2, 1, TileType::DOOR);
    ASSERT_TRUE(m.set_door_state(2, 1, DoorState::CLOSED));

    // 玩家 rect 在 tile (1,1) 中心, 向右移动 (前缘到达 x=64 即 tile 2)
    Rectangle r = { 32.0f, 32.0f, 32.0f, 32.0f };
    EXPECT_TRUE(m.is_rect_walkable(r));   // tile(1,1) 是 FLOOR 可走

    // 门当前 CLOSED: 不可走、挡视线
    EXPECT_FALSE(m.is_walkable(2, 1));
    EXPECT_TRUE(m.blocks_sight(2, 1));

    // 触发接触开门 (rect 前缘 r.x+width=64 指向 tile 2 = 门)
    bool opened = m.try_open_door_toward(r, 1.0f, 0.0f);
    EXPECT_TRUE(opened);
    EXPECT_EQ(m.door_state_at(2, 1), DoorState::OPEN);
    EXPECT_TRUE(m.is_walkable(2, 1));
    EXPECT_FALSE(m.blocks_sight(2, 1));
}

// 向左 / 上 / 下 三方向同样生效
TEST(DoorInteract, ContactOpen_AllDirections) {
    // 左: door 在 x=2, 玩家在 x=3 向左
    GameMap m1(6, 3, 32);
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 6; x++)
            m1.set_tile(x, y, TileType::FLOOR);
    m1.set_tile(3, 1, TileType::DOOR);
    ASSERT_TRUE(m1.set_door_state(3, 1, DoorState::CLOSED));
    Rectangle rl = { 4*32.0f + 1.0f, 32.0f, 32.0f, 32.0f };  // 前缘 4*32+1 -> tile4 左
    EXPECT_TRUE(m1.try_open_door_toward(rl, -1.0f, 0.0f));
    EXPECT_EQ(m1.door_state_at(3, 1), DoorState::OPEN);

    // 上: door 在 y=2, 玩家在 y=3 向上
    GameMap m2(3, 6, 32);
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 3; x++)
            m2.set_tile(x, y, TileType::FLOOR);
    m2.set_tile(1, 2, TileType::DOOR);
    ASSERT_TRUE(m2.set_door_state(1, 2, DoorState::CLOSED));
    Rectangle ru = { 32.0f, 3*32.0f + 1.0f, 32.0f, 32.0f };
    EXPECT_TRUE(m2.try_open_door_toward(ru, 0.0f, -1.0f));
    EXPECT_EQ(m2.door_state_at(1, 2), DoorState::OPEN);

    // 下: door 在 y=2, 玩家在 y=1 向下
    GameMap m3(3, 6, 32);
    for (int y = 0; y < 6; y++)
        for (int x = 0; x < 3; x++)
            m3.set_tile(x, y, TileType::FLOOR);
    m3.set_tile(1, 2, TileType::DOOR);
    ASSERT_TRUE(m3.set_door_state(1, 2, DoorState::CLOSED));
    Rectangle rd = { 32.0f, 1*32.0f, 32.0f, 32.0f };  // 玩家在 tile(1,1), 向下
    EXPECT_TRUE(m3.try_open_door_toward(rd, 0.0f, 1.0f));
    EXPECT_EQ(m3.door_state_at(1, 2), DoorState::OPEN);
}

// 非 door / OPEN door 不触发开门; 静止不触发
TEST(DoorInteract, NoOpen_OnNonDoorOrOpen) {
    GameMap m(6, 3, 32);
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 6; x++)
            m.set_tile(x, y, TileType::FLOOR);
    // WALL 在 x=2 (非 door)
    m.set_tile(2, 1, TileType::WALL);
    Rectangle r = { 32.0f, 32.0f, 32.0f, 32.0f };
    EXPECT_FALSE(m.try_open_door_toward(r, 1.0f, 0.0f));  // 撞墙不开

    // OPEN door 不动
    m.set_tile(4, 1, TileType::DOOR);
    EXPECT_EQ(m.door_state_at(4, 1), DoorState::OPEN);
    Rectangle r2 = { 3*32.0f, 32.0f, 32.0f, 32.0f };
    EXPECT_FALSE(m.try_open_door_toward(r2, 1.0f, 0.0f));  // OPEN door 无需"开"

    // 静止 (mx=my=0) 不触发
    EXPECT_FALSE(m.try_open_door_toward(r, 0.0f, 0.0f));
}

// CLOSED door: is_walkable=false, blocks_sight=true; OPEN 恢复
TEST(DoorInteract, ClosedDoor_BlocksMovementAndSight) {
    GameMap m(3, 3, 32);
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++)
            m.set_tile(x, y, TileType::FLOOR);
    m.set_tile(1, 1, TileType::DOOR);
    ASSERT_TRUE(m.set_door_state(1, 1, DoorState::CLOSED));
    EXPECT_FALSE(m.is_walkable(1, 1));
    EXPECT_TRUE(m.blocks_sight(1, 1));
    // FOV: 从 (0,1) 看 (2,1) 被 CLOSED 门挡住
    m.update_fov(0, 1, 3);
    EXPECT_TRUE(m.isVisible(1, 1));   // 门 tile 自身可见
    EXPECT_FALSE(m.isVisible(2, 1));  // 门后不可见
}

// 生成的地图门默认全部 OPEN (Batch 2B 保持 gameplay 不变)
TEST(DoorInteract, GeneratedMap_DoorsDefaultOpen) {
    DungeonGenerator gen(MAP_WIDTH, MAP_HEIGHT, TILE_SIZE);
    auto map = gen.generate(42, 3, 0);
    int doors = 0;
    for (int y = 0; y < MAP_HEIGHT; y++)
        for (int x = 0; x < MAP_WIDTH; x++)
            if (map->tile_at(x, y) == TileType::DOOR) {
                doors++;
                EXPECT_EQ(map->door_state_at(x, y), DoorState::OPEN)
                    << "door (" << x << "," << y << ") should be OPEN";
            }
    EXPECT_GT(doors, 0);
}
