# Phase 0 可见性系统架构审计

## A. 地图数据结构分析

### GameMap（game_map.h:5）
- **Tile 定义**: `struct Tile { TileType type; bool is_walkable; }` — 只有两个字段
- **内部存储**: `std::vector<std::vector<Tile>> _tiles` — 40×30 = 1200 tiles（TYPE_COUNT=40, ROW_COUNT=30）
- **公共接口**:
  - `isWalkable(x, y)` / `isWalkablePixel(px, py)` — 碰撞查询
  - `getTileType(x, y)` / `getTileTypePixel(px, y)` — 类型查询
  - `PixelToWorld(px, py)` / `WorldToPixel(wx, wy)` — 坐标转换
  - `setTile(x, y, type)` / `setWalkable(x, y, w)` — 修改
  - `reset()` / `randomize()` / `add_walls()` — 地图生成辅助

### DungeonGenerator（dungeon_generator.h:6）
- `vector<Rectangle> _rooms` — BSP 分割的矩形房间列表（只存矩形，无 Door/RoomID）
- `BSPNode` — BSP 树节点（left/right/parent/children）
- `generateDungeon()` — 一次生成完整地图，返回 `GameMap`
- `getSpawnPoint()` — 随机选一个 walkable tile 作为玩家出生点

**结论**: 地图是扁平的 `vector<vector<Tile>>`，无房间隔离，无可见性字段。可见性状态需要新增存储。

---

## B. 玩家位置→地图坐标调用链

### 坐标体系
```
像素坐标 (float) ←→ 网格坐标 (int, tile_x/tile_y)
PixelToWorld: px / TILE_SIZE(32) → tile_x
WorldToPixel: wx * TILE_SIZE(32) → px
```

### 关键调用点
| 文件 | 行号 | 用途 |
|------|------|------|
| `entity.h:34` | `getX()/getY()` | 获取像素坐标 |
| `entity.h:60` | `can_move_to(nx, ny, map)` | 碰撞检测 |
| `entity.cpp:8` | `PixelToWorld` → `getTileType` | 地图类型查询 |
| `player_controller.cpp:36` | `getX()/getY()` | 玩家移动 |
| `player_controller.cpp:129` | `can_attack_aoe/map/rect` | 战斗范围 |
| `game_renderer.cpp:151-170` | `getX()/getY()` | 实体渲染 |
| `camera.cpp:43-49` | `getX()/getY()` | 相机跟随 |

### 坐标转换频率
- **高频** (每帧): `getX()/getY()` — 100+ 调用
- **中频** (移动/碰撞): `PixelToWorld` → `isWalkable` — 30+ 调用
- **低频** (生成): `WorldToPixel` — 少数调用

**结论**: 可见性查询需要在高频路径上高效。用 `Tile` 上的 bool 字段或独立的 `vector<vector<bool>>` 都满足 O(1) 查询。

---

## C. 渲染管线分析

### 当前渲染顺序（game_renderer.cpp:66-322）
```
1. BeginDrawing
2. ClearBackground
3. DrawMinimap        ← 左下角迷你地图
4. DrawFloor          ← 地砖渲染（3层: type3→type1→type2）
5. DrawWall           ← 墙壁纹理渲染
6. DrawEntity         ← 按 y 排序的实体渲染
7. DrawPlayerHealthBar ← 血条
8. DrawUI             ← 技能冷却/主动技能
9. DrawEventDialog    ← 剧情对话框
10. EndDrawing
```

### Camera 机制
- **无 Camera2D**: 手动 `Camera::getOffset()` + `DrawRectangleRec()`/`DrawTextureRec()`
- **Offset 计算**: `offset = camera_pos - screen_center + camera_offset` (game_renderer.cpp:119-123)
- **应用方式**: 每个 DrawRectangle/DrawTextureRec 都加 offset

### 地砖渲染细节（game_renderer.cpp:273-322）
```
DrawFloor:  type==3 地毯(半透明) → type==1 地砖(半透明) → type==2 草地
DrawWall:   type==0 墙壁(完整不透明)
```

**结论**: 插入点在 `DrawFloor()` 之前（屏幕填充黑色后），逐 tile 渲染时根据可见性状态决定是否绘制。不需要改渲染架构。

---

## D. 房间系统分析

### 当前房间实现
- `DungeonGenerator::getRooms()` 返回 `vector<Rectangle>` — 只是矩形列表
- 无 `Room` 类、无 `Door`、无房间 ID、无进入/退出事件
- 房间间通过 walkable tile 连接，无门禁

### 战斗触发
- `game_scene_combat.cpp:81` — 每步随机 25% 概率触发战斗
- **无房间锁定**: 走在任何 walkable tile 都可能触发战斗
- 无 `in_combat` 状态机（战斗通过 `_battle_cards` 临时状态）

### 房间发现 vs 战斗触发
- **可见性**: 走到房间附近 → 房间变亮（可见性系统）
- **战斗触发**: 保持现有随机概率逻辑，不改

**结论**: 不需要 Room 类或 Door。可见性系统与房间系统解耦——可见性只管 "这个 tile 是否被探索过"，不管房间边界。

---

## E. AI/Navigation 依赖分析

### AI 使用地图数据的位置
| AI 类型 | 文件 | 使用方式 | 影响 |
|---------|------|----------|------|
| Monster AI | `ai.cpp:730` | `map.isWalkable(nx,ny)` | 只读 walkability |
| Monster AI | `ai.cpp:440` | `can_move_to(nx,ny,map)` | 只读 walkability |
| Boss AI | `boss.cpp:177` | `map.isWalkable()` | 只读 walkability |
| Seeker AI | `seeker_ai.cpp:210` | `map.isWalkable()` | 只读 walkability |
| Sim AI | `sim_ai.cpp:790-1400` | `TileType` 读取 | 只读 START/STORE/WALL 房间 |

### 无 AI 依赖现有可见性
- **默认行为**: 所有 AI 假设全局可见（怪物始终能看到玩家，玩家始终能看到怪物）
- **可见性启用后**: 需要决定 AI 行为：
  1. **方案 A (简单)**: AI 不感知可见性，怪物始终"知道"玩家位置（渲染隐藏，AI 不隐藏）
  2. **方案 B (严格)**: AI 仅在可见时行动（需要改 AI 逻辑）
  3. **方案 C (混合)**: AI 始终行动，但只在可见时渲染

**推荐**: 方案 A（最简单，不改 AI 逻辑），可见性 = 纯渲染层。

---

## F. 数据所有权建议

### 方案 1: Tile 扩展（推荐）
```cpp
struct Tile {
    TileType type;
    bool is_walkable;
    bool is_visible;   // 当前帧是否可见（实时 FOV）
    bool is_explored;  // 是否曾被探索过（永久记忆）
};
```
- **优点**: 最少改动，所有地图查询自动包含可见性
- **缺点**: Tile 职责略增，但 2 个 bool 开销极小（+2 bytes/tile = 2400 bytes）

### 方案 2: 独立可见性网格
```cpp
class VisibilityGrid {
    std::vector<std::vector<bool>> _visible;
    std::vector<std::vector<bool>> _explored;
};
```
- **优点**: 关注点分离，可见性逻辑独立
- **缺点**: 需要额外同步 GameMap 坐标，渲染时需要两个数据源

### 方案 3: 仅渲染层
- 不存状态，每帧重新计算可见性
- **优点**: 无内存开销
- **缺点**: 每帧计算 1200 tiles，性能浪费

**推荐**: **方案 1 (Tile 扩展)** — 最少改动、最高性能、最低维护成本。

---

## G. Phase 1 实施计划

### 目标
添加 Field of View (FOV) 和迷雾系统。玩家视野范围内的 tile 标记为 visible，视野外已探索的 tile 标记为 explored（显示灰色）。

### 依赖
- 无新文件依赖
- 无新 JSON 配置
- 无新 GitHub Actions 步骤
- 不改现有测试

### 数据流
```
玩家移动 (player_controller.cpp)
    → GameMap::update_fov(player_tile_x, player_tile_y, fov_radius)
        → 逐 tile 射线检测 (Raycast FOV)
        → 标记 is_visible / is_explored
    → GameRenderer::DrawFloor/DrawWall 检查 is_visible/is_explored
        → visible: 正常渲染
        → explored && !visible: 灰色半透明覆盖
        → !explored: 不渲染（黑色）
```

### 新增字段（Tile）
```cpp
struct Tile {
    TileType type;
    bool is_walkable;
    bool is_visible = false;   // 当前帧可见
    bool is_explored = false;  // 已探索
};
```

### 新增方法（GameMap）
```cpp
void GameMap::update_fov(int center_x, int center_y, int radius);
bool GameMap::isVisible(int x, int y) const;
bool GameMap::isExplored(int x, int y) const;
```

### 渲染修改（GameRenderer）
- `DrawFloor()`: 检查 tile 可见性，不可见不渲染，已探索灰色覆盖
- `DrawWall()`: 同上
- `DrawEntity()`: 只渲染 visible tile 上的实体（或 visible 范围内的实体）

### FOV 算法选择
- **简单射线投射**: 从玩家向 360° 发射射线，检测 walkability 阻挡
- **递归阴影投射**: 性能更优，但实现复杂
- **推荐**: 简单射线投射，1200 tiles + 射线数可控

### 验证
- `cmake --build build --config Release` 成功
- `ctest --test-dir build` 35/35 通过
- 300-sim win rate 无异常变化
- 运行时迷雾效果正确：脚下亮，视野内正常，视野外灰色，未探索黑色

### 不做
- 不改 AI 行为
- 不改战斗触发逻辑
- 不改房间结构
- 不改游戏循环
- 不新增 JSON 配置
- 不新增测试文件
