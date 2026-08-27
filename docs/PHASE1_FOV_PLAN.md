# Phase 1: 基础 FOV 迷雾系统 — 修订实施计划

## 目标
玩家移动时逐步探索地图。三种渲染状态：
- **未探索**: 黑色（不渲染）
- **当前可见**: 正常渲染
- **曾探索但当前不可见**: 昏暗渲染（亮度降 40%）

## 约束
- 不改 AI/Navigation/Combat 行为
- 不改战斗触发逻辑
- 不改房间结构
- 不改游戏循环
- 不新增 JSON 配置
- FOV 不与永久照明语义耦死
- `blocks_sight()` ≠ `is_walkable() == false`（仅 Phase 1 内部临时使用墙壁类型判断）

---

## 阶段 1: Tile 扩展 + blocks_sight

### 文件: `src/game/world/game_map.h`

```cpp
// Tile 新增字段
struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;
    bool is_visible = false;    // 当前帧是否可见
    bool is_explored = false;   // 是否曾被探索过

    static Tile floor()  { return {TileType::FLOOR, true, false, false}; }
    static Tile wall()   { return {TileType::WALL, false, false, false}; }
    static Tile stairs() { return {TileType::STAIRS_DOWN, true, false, false}; }
    static Tile lava()   { return {TileType::LAVA, true, false, false}; }
};
```

### 文件: `src/game/world/game_map.h` — GameMap 新增

```cpp
// FOV 查询
bool isVisible(int x, int y) const;
bool isExplored(int x, int y) const;

// 视线遮挡 (Phase 1: 内部临时使用墙壁类型)
bool blocks_sight(int x, int y) const;

// FOV 更新 (仅在玩家跨 tile 时调用)
void update_fov(int center_x, int center_y, int radius);

// 新层初始化 (reset/reload 时调用)
void reset_visibility();
```

### 文件: `src/game/world/game_map.cpp` — 实现

```cpp
bool GameMap::isVisible(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_visible;
}

bool GameMap::isExplored(int x, int y) const {
    return _in_bounds(x, y) && _tiles[y][x].is_explored;
}

bool GameMap::blocks_sight(int x, int y) const {
    if (!_in_bounds(x, y)) return true;  // 边界外视为遮挡
    // Phase 1: 临时使用墙壁类型判断
    // 未来可扩展为独立的 blocks_sight 字段（支持半透明/可破坏墙壁）
    return _tiles[y][x].type == TileType::WALL;
}

void GameMap::reset_visibility() {
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            _tiles[y][x].is_visible = false;
            _tiles[y][x].is_explored = false;
        }
}
```

---

## 阶段 2: FOV 算法（增量更新）

### 算法: 简单射线投射
从玩家位置向 360° 发射射线，每 1° 一条（共 360 条）。射线沿 tile 逐步前进，遇到 `blocks_sight` 的 tile 停止。

### 触发条件
仅在以下情况调用 `update_fov()`:
1. 玩家跨越 tile 边界（`pixel_to_tile` 坐标变化）
2. 地图变化（`set_tile` 时可选触发）

### 在 GameScene 中追踪状态

```cpp
// game_scene.h 新增成员
int _last_player_tile_x = -1;
int _last_player_tile_y = -1;
static constexpr int FOV_RADIUS = 8;  // 可视半径（tile 数）
```

### 更新点: `game_scene.cpp` — `_process()` 中

```cpp
// 玩家位置变化时更新 FOV
if (player && game_map) {
    auto [tx, ty] = game_map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);
    if (tx != _last_player_tile_x || ty != _last_player_tile_y) {
        _last_player_tile_x = tx;
        _last_player_tile_y = ty;
        game_map->update_fov(tx, ty, FOV_RADIUS);
    }
}
```

### 新层进入时

```cpp
// enter_floor() 或 load_saved_game() 中
_last_player_tile_x = -1;
_last_player_tile_y = -1;
game_map->reset_visibility();
// 首次 FOV 计算在下一帧 _process 中自动触发
```

### FOV 实现（GameMap）

```cpp
void GameMap::update_fov(int cx, int cy, int radius) {
    // 1. 清除所有 is_visible
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            _tiles[y][x].is_visible = false;

    // 2. 射线投射 (360 条, 每度一条)
    for (int deg = 0; deg < 360; deg++) {
        float rad = deg * DEG2RAD;
        float dx = cosf(rad);
        float dy = sinf(rad);

        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + (int)roundf(dx * dist);
            int ty = cy + (int)roundf(dy * dist);
            if (!_in_bounds(tx, ty)) break;

            _tiles[ty][tx].is_visible = true;
            _tiles[ty][tx].is_explored = true;

            if (blocks_sight(tx, ty)) break;  // 墙壁遮挡后停止
        }
    }
}
```

---

## 阶段 3: 主地图渲染修改

### 文件: `src/game/world/game_map.cpp` — `draw()` 修改

在现有绘制逻辑中，对每个 tile 增加可见性检查：

```cpp
// 在 draw() 的 tile 循环中，现有逻辑之前插入
const auto& t = _tiles[y][x];

// Phase 1 可见性: 未探索 → 跳过
if (!t.is_explored) continue;

// 计算亮度
float brightness = t.is_visible ? 1.0f : 0.6f;
Color dim = {
    (unsigned char)(original_color.r * brightness),
    (unsigned char)(original_color.g * brightness),
    (unsigned char)(original_color.b * brightness),
    255
};
```

### 具体修改点

`game_map.cpp:119-268` 的 tile 循环中：

1. **墙壁** (line 125): 未探索跳过，已探索灰色渲染
2. **地板** (line 133): 未探索跳过，已探索灰色渲染
3. **楼梯** (line 208): 同上
4. **熔岩** (line 212): 同上
5. **事件标记** (line 224): 仅在可见时显示
6. **Arena 元素** (line 232): 仅在可见时显示

---

## 阶段 4: 实体渲染规则

### 规则
实体中心所在 tile `is_visible` → 渲染，否则隐藏。

### 修改点: `game_scene.cpp` — `_draw_entities()`

```cpp
// line 2170: 怪物循环中增加检查
for (auto& m : monsters) {
    // Phase 1: 实体中心 tile 不可见 → 跳过渲染
    if (game_map) {
        auto [mtx, mty] = game_map->pixel_to_tile(
            m->entity.rect.x + m->entity.rect.width / 2,
            m->entity.rect.y + m->entity.rect.height / 2);
        if (!game_map->isVisible(mtx, mty)) continue;
    }
    m->draw(_cam_x, _cam_y);
    // ... 后续逻辑
}

// line 2218: 玩家始终渲染
if (player) player->draw_no_cam(_cam_x, _cam_y);
```

### NPC (line 2225): 同样检查 tile 可见性

### Ground Items (line 2268): 同样检查 tile 可见性

---

## 阶段 5: 单元测试

### 文件: `tests/world/fov_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "game_map.h"

class FOVTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 10x10 地图，中间有墙壁
        map = GameMap(10, 10, 32);
        // 全部设为地板
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++)
                map.set_tile(x, y, TileType::FLOOR);
        // 在 (5,5) 放墙壁
        map.set_tile(5, 5, TileType::WALL);
    }
    GameMap map;
};

// T1: 玩家所在 tile visible + explored
TEST_F(FOVTest, PlayerTileVisibleAndExplored) {
    map.update_fov(3, 3, 5);
    EXPECT_TRUE(map.isVisible(3, 3));
    EXPECT_TRUE(map.isExplored(3, 3));
}

// T2: 半径内无遮挡 tile visible
TEST_F(FOVTest, VisibleWithinRadius) {
    map.update_fov(3, 3, 5);
    EXPECT_TRUE(map.isVisible(4, 3));  // 相邻
    EXPECT_TRUE(map.isVisible(3, 4));
    EXPECT_TRUE(map.isVisible(2, 3));  // 反方向
}

// T3: 墙壁后 tile 不可见
TEST_F(FOVTest, BlockedByWall) {
    map.update_fov(3, 3, 5);
    // 墙在 (5,5), 射线从 (3,3) 到 (6,6) 应被 (5,5) 阻挡
    EXPECT_FALSE(map.isVisible(6, 6));
}

// T4: 墙本身可见
TEST_F(FOVTest, WallItselfVisible) {
    map.update_fov(3, 3, 5);
    EXPECT_TRUE(map.isVisible(5, 5));  // 墙本身在视野内
    EXPECT_TRUE(map.isExplored(5, 5));
}

// T5: 离开后 tile 从 visible 变为 explored
TEST_F(FOVTest, LeavesTileBecomesExplored) {
    map.update_fov(3, 3, 5);
    EXPECT_TRUE(map.isVisible(3, 3));

    // 移动到 (8, 8)
    map.update_fov(8, 8, 5);
    EXPECT_FALSE(map.isVisible(3, 3));   // 不再可见
    EXPECT_TRUE(map.isExplored(3, 3));   // 仍被探索
}

// T6: reset 后探索状态清空
TEST_F(FOVTest, ResetClearsExploration) {
    map.update_fov(3, 3, 5);
    EXPECT_TRUE(map.isExplored(3, 3));

    map.reset_visibility();
    EXPECT_FALSE(map.isVisible(3, 3));
    EXPECT_FALSE(map.isExplored(3, 3));
}

// T7: blocks_sight 独立于 is_walkable
TEST_F(FOVTest, BlocksSightIndependentOfWalkable) {
    map.set_tile(7, 7, TileType::LAVA);  // 可走但不遮挡视线
    EXPECT_TRUE(map.is_walkable(7, 7));
    EXPECT_FALSE(map.blocks_sight(7, 7));

    EXPECT_TRUE(map.blocks_sight(5, 5));  // 墙壁遮挡
    EXPECT_FALSE(map.is_walkable(5, 5));
}

// T8: 边界外视为遮挡
TEST_F(FOVTest, OutOfBoundsBlocksSight) {
    EXPECT_TRUE(map.blocks_sight(-1, 0));
    EXPECT_TRUE(map.blocks_sight(10, 5));
}
```

### 注册测试: `tests/CMakeLists.txt`

```cmake
add_roguelike_test(fov_test world/fov_test.cpp)
```

---

## 预估修改文件清单

| 文件 | 改动类型 | 改动量 |
|------|----------|--------|
| `src/game/world/game_map.h` | 修改 | Tile +4 bool, GameMap +5 方法声明 |
| `src/game/world/game_map.cpp` | 修改 | +5 方法实现, draw() 可见性检查 |
| `src/game/scenes/game_scene.h` | 修改 | +2 成员 (_last_player_tile_x/y), +1 常量 |
| `src/game/scenes/game_scene.cpp` | 修改 | _process() FOV更新, _draw_entities() 可见性检查, enter_floor() reset |
| `tests/world/fov_test.cpp` | 新建 | 8 个测试用例 |
| `tests/CMakeLists.txt` | 修改 | +1 行测试注册 |

**预估总行数**: ~250 行（含测试）

---

## 验证清单

- [ ] `cmake --build build --config Release` 成功
- [ ] `ctest --test-dir build` 35+ 测试全部通过（含新增 fov_test）
- [ ] F1: 主地图初始黑暗
- [ ] F2: 玩家移动时逐步探索
- [ ] F3: 墙壁后怪物完全隐藏
- [ ] F4: 离开区域后保持昏暗可见
- [ ] F5: AI/Navigation/Combat 行为无变化
- [ ] F6: blocks_sight() ≠ is_walkable()（LAVA 可走不遮挡，Wall 不可走遮挡）

## 不做
- 永久照明/火把系统
- 光源接口
- 实体 bounds-based visibility
- 房间系统改动
- JSON 配置
