# Phase 2 Pre-Implementation Trace

> 状态：编码前最终 Trace（只读分析）
> 前置：Phase 2 设计方案已确认 (B+ Door Tile)
> 地图参数：40×30, min_part=8, min_room=5, margin=1, TILE_SIZE=32

---

## 1. BSP 分割真实坐标 Trace

### 1.1 分割树（确定性示例）

```
Root: (0,0,40,30)  w>h → vertical split, split=20
├─ Left:  (0,0,20,30)  h>w → horizontal split, split=15
│  ├─ Top-Left:     (0,0,20,15)  w>h → vertical split, split=10
│  │  ├─ Leaf A: (0,0,10,15)    h=15<16 → STOP
│  │  └─ Leaf B: (10,0,10,15)   h=15<16 → STOP
│  └─ Bottom-Left:  (0,15,20,15)  w>h → vertical split, split=10
│     ├─ Leaf C: (0,15,10,15)   → STOP
│     └─ Leaf D: (10,15,10,15)  → STOP
└─ Right: (20,0,20,30)  h>w → horizontal split, split=15
   ├─ Top-Right:    (20,0,20,15)  w>h → vertical split, split=10
   │  ├─ Leaf E: (20,0,10,15)   → STOP
   │  └─ Leaf F: (30,0,10,15)   → STOP
   └─ Bottom-Right: (20,15,20,15)  w>h → vertical split, split=10
      ├─ Leaf G: (20,15,10,15)  → STOP
      └─ Leaf H: (30,15,10,15)  → STOP
```

**8 leaves = 8 rooms, 7 internal nodes = 7 corridors**

### 1.2 Room 放置（Leaf A 详细 trace）

```
Leaf A: (0,0,10,15)
  rw = 5 + rand(max(1, 10-2-5+1)) = 5 + rand(4) → 假设 rw=6
  rh = 5 + rand(max(1, 15-2-5+1)) = 5 + rand(9) → 假设 rh=8
  rx = 0 + 1 + rand(max(1, 10-6-2+1)) = 1 + rand(3) → 假设 rx=2
  ry = 0 + 1 + rand(max(1, 15-8-2+1)) = 1 + rand(6) → 假设 ry=3

  Room A rect: (2, 3, 6, 8)
  Interior: columns 2-7, rows 3-10 (6×8=48 floor tiles)
```

### 1.3 Room A 的 Tile 分布

```
     0    1    2    3    4    5    6    7    8    9
  0: #    #    #    #    #    #    #    #    #    #
  1: #    #    #    #    #    #    #    #    #    #
  2: #    #    #    #    #    #    #    #    #    #  ← Room A top wall
  3: #    #    .    .    .    .    .    .    #    #  ← Room A row 3
  4: #    #    .    .    .    .    .    .    #    #  ← Room A row 4
  5: #    #    .    .    .    .    .    .    #    #  ← Room A row 5
  6: #    #    .    .    .    .    .    .    #    #  ← Room A row 6
  7: #    #    .    .    .    .    .    .    #    #  ← Room A row 7
  8: #    #    .    .    .    .    .    .    #    #  ← Room A row 8
  9: #    #    .    .    .    .    .    .    #    #  ← Room A row 9
 10: #    #    .    .    .    .    .    .    #    #  ← Room A row 10
 11: #    #    #    #    #    #    #    #    #    #  ← Room A bottom wall
 12: #    #    #    #    #    #    #    #    #    #
 13: #    #    #    #    #    #    #    #    #    #
 14: #    #    #    #    #    #    #    #    #    #

  Legend:  # = WALL (walkable=false)    . = FLOOR (walkable=true)
```

**Room 边界精确坐标：**
- Top wall:    row 2,  columns 2-7 (6 tiles)
- Bottom wall: row 11, columns 2-7 (6 tiles)
- Left wall:   column 1, rows 3-10 (8 tiles)
- Right wall:  column 8, rows 3-10 (8 tiles)
- **Room 墙壁 = 1 tile 厚，和地图其他 WALL 完全相同**

---

## 2. Door 位置精确坐标

### 2.1 _pick_room_edge 逻辑

```
输入: BSPNode* node (包含 Room), target_x, target_y (另一个房间的中心)
输出: (edge_x, edge_y) — 房间内部紧邻墙壁的地板 tile

算法:
  room = (rx=2, ry=3, rw=6, rh=8)
  
  候选边缘中点:
    上: (rx + rw/2, ry)     = (5, 3)   — row 3, column 5
    下: (rx + rw/2, ry+rh-1)= (5, 10)  — row 10, column 5
    左: (rx, ry + rh/2)     = (2, 7)   — row 7, column 2
    右: (rx+rw-1, ry+rh/2)  = (7, 7)   — row 7, column 7
  
  过滤: Door 位置必须在地图内 (0..39, 0..29)
    上 Door: (5, 2)   — ✓ 在地图内
    下 Door: (5, 11)  — ✓ 在地图内
    左 Door: (1, 7)   — ✓ 在地图内
    右 Door: (8, 7)   — ✓ 在地图内
  
  选择: 距离 (target_x, target_y) 最近的
```

### 2.2 _compute_door_pos 逻辑

```
输入: edge_point (5, 3), direction (向上)
输出: door_point (5, 2)

规则: Door = edge_point 向房间外侧偏移 1 格
  edge_point 是房间内部地板 tile
  door_point 是房间外部墙壁 tile

  方向映射:
    上 edge (5,3)  → Door (5,2)   — row-1
    下 edge (5,10) → Door (5,11)  — row+1
    左 edge (2,7)  → Door (1,7)   — col-1
    右 edge (7,7)  → Door (8,7)   — col+1
```

### 2.3 Door 位置在模板中的表示

```
以 Room A 右边缘为例:

  原始模板 (只有 # 和 .):
  row 6: #    #    .    .    .    .    .    .    #    #
  row 7: #    #    .    .    .    .    .    .    #    #  ← 右边缘 (7,7)
  row 8: #    #    .    .    .    .    .    .    #    #

  放置 Door 后:
  row 6: #    #    .    .    .    .    .    .    #    #
  row 7: #    #    .    .    .    .    .    .    D    #  ← Door (8,7)
  row 8: #    #    .    .    .    .    .    .    #    #
                                    edge(7,7) ^  door(8,7) ^
```

**Door 位于 Room 墙壁 tile 上，紧邻 Room 内部地板。**

---

## 3. 走廊连接完整坐标 Trace

### 3.1 场景设定

```
Room A: (2, 3, 6, 8)  — Leaf A
Room B: (14, 5, 6, 6) — Leaf B (兄弟节点)

Room A center: (5, 7)
Room B center: (17, 8)

BSP 连接: Root → Left → Leaf A ↔ Leaf B
```

### 3.2 _pick_room_edge 追踪

```
_pick_room_edge(Leaf_A, target=Room_B_center=(17,8)):
  Room A 候选: 上(5,3) 下(5,10) 左(2,7) 右(7,7)
  距离 (17,8):
    上: sqrt((17-5)²+(8-3)²) = sqrt(144+25) = 13.0
    下: sqrt((17-5)²+(8-10)²) = sqrt(144+4) = 12.2
    左: sqrt((17-2)²+(8-7)²) = sqrt(225+1) = 15.0
    右: sqrt((17-7)²+(8-7)²) = sqrt(100+1) = 10.0  ← 最近
  → edge_a = (7, 7)

_pick_room_edge(Leaf_B, target=Room_A_center=(5,7)):
  Room B 候选: 上(17,5) 下(17,10) 左(14,8) 右(19,8)
  距离 (5,7):
    上: sqrt((5-17)²+(7-5)²) = sqrt(144+4) = 12.2
    下: sqrt((5-17)²+(7-10)²) = sqrt(144+9) = 12.4
    左: sqrt((5-14)²+(7-8)²) = sqrt(81+1) = 9.0  ← 最近
    右: sqrt((5-19)²+(7-8)²) = sqrt(196+1) = 14.0
  → edge_b = (14, 8)
```

### 3.3 _compute_door_pos 追踪

```
Room A 右边缘 (7,7) → 方向向右 → Door A = (8, 7)
Room B 左边缘 (14,8) → 方向向左 → Door B = (13, 8)
```

### 3.4 走廊路径

```
当前 _carve_corridor 的输入: (8,7) → (13,8)
L 形路径 (随机选择水平优先):
  水平段: (8,7) → (13,7) — 6 tiles
  垂直段: (13,7) → (13,8) — 2 tiles

走廊中心点: (8,7), (9,7), (10,7), (11,7), (12,7), (13,7), (13,8)
```

### 3.5 Diamond Carving 追踪 (radius=1)

```
走廊中心点 → diamond 雕刻:

(8,7):  雕刻 (7,7),(9,7),(8,6),(8,8),(8,7)
        (7,7) = Room A 内部 '.' → 已是地板，无需改变
        (8,7) = Door 位置 '#' → 雕刻为 '.' → 后续设为 'D'
        (9,7) = 墙壁 '#' → 雕刻为 '.'
        (8,6) = 墙壁 '#' → 雕刻为 '.'
        (8,8) = 墙壁 '#' → 雕刻为 '.'

(9,7):  雕刻 (8,7),(10,7),(9,6),(9,8),(9,7)
        全部是 '#' → 雕刻为 '.'

... (类似过程)

(13,8): 雕刻 (12,8),(14,8),(13,7),(13,9),(13,8)
        (14,8) = Room B 内部 '.' → 已是地板
        (13,8) = Door 位置 → 后续设为 'D'
```

### 3.6 最终模板

```
     0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15   16   17   18   19
  5: #    #    #    #    #    #    #    #    #    #    #    #    #    #    .    .    .    .    .    #
  6: #    #    .    .    .    .    .    .    #    #    #    #    #    #    .    .    .    .    .    #
  7: #    #    .    .    .    .    .    .    D ←── 走廊 ──────────────→ D    .    .    .    .    .    .    #
  8: #    #    .    .    .    .    .    .    .    .    .    .    .    #    .    .    .    .    .    #
  9: #    #    .    .    .    .    .    .    #    #    #    #    #    #    .    .    .    .    .    #
 10: #    #    .    .    .    .    .    .    #    #    #    #    #    #    .    .    .    .    .    #

  Legend:  # = WALL    . = FLOOR    D = DOOR
  走廊 = columns 8-13, rows 6-8 (菱形，宽度 3 tiles)
```

**关键验证：**
- ✅ Room A 内部 (columns 2-7) 未被走廊破坏
- ✅ Room B 内部 (columns 14-19) 未被走廊破坏
- ✅ Door A (8,7) 在 Room A 右墙壁上
- ✅ Door B (13,8) 在 Room B 左墙壁上
- ✅ 走廊不穿过任何 Room interior

---

## 4. 关键设计问题解答

### 4.1 新走廊不会进入 Room interior 吗？

**需要修改 `_carve_diamond`。**

当前代码：
```cpp
void DungeonGenerator::_carve_diamond(grid, cx, cy, r) {
    for (dy = -r; dy <= r; dy++)
        for (dx = -r; dx <= r; dx++) {
            if (abs(dx)+abs(dy) > r) continue;
            int tx = cx+dx, ty = cy+dy;
            if (in_bounds) g[ty][tx] = '.';  // ← 无条件雕刻
        }
}
```

**问题：** Diamond radius=2 时，(8,7) 的 diamond 会覆盖 (7,7)（Room A 内部），因为 |7-8|+|7-7|=1≤2。这会破坏 Room interior。

**修复方案（最小侵入）：** 只雕刻 '#' tile，跳过已存在的 '.'。

```cpp
if (in_bounds && g[ty][tx] == '#')  // ← 仅新增这一行条件
    g[ty][tx] = '.';
```

**效果：**
- 走廊穿过墙壁 '#' → 雕刻为 '.' ✓
- 走廊经过 Room interior '.' → 跳过，不改变 ✓
- 走廊经过已有走廊 '.' → 跳过，不改变 ✓

### 4.2 Door 不会悬空吗？

**不会。** Door 的位置经过严格验证：

1. `_pick_room_edge` 返回 Room 内部地板 tile（紧邻墙壁）
2. `_compute_door_pos` 从 edge 向外偏移 1 格，得到墙壁 tile
3. 检查 Door 在地图边界内 (0..39, 0..29)
4. Door 一侧是 Room interior (edge tile)，另一侧是 Corridor

**Door 连接关系：**
```
[Room interior floor] ← edge_point
         |
      [Door] ← door_point (墙壁 tile → 'D')
         |
    [Corridor floor]
```

### 4.3 Door 不会生成在地图边界吗？

**需要边界检查。**

极端情况：Room 在地图边缘（如 rx=1），左墙壁在 column 0，Door 在 column -1 → 越界。

**解决方案：** `_pick_room_edge` 过滤 Door 越界的边缘。

```
候选边缘 → 计算 Door 位置 → 检查 (0..39, 0..29)
→ 越界的边缘剔除 → 从剩余边缘中选最近的
→ 若所有边缘都越界（理论上不会，因为 Room 有 margin），跳过连接
```

### 4.4 FOV 对 Door 的影响

**无影响。**

Door 属性：
- `type = TileType::DOOR` (≠ WALL)
- `blocks_sight()` → `type == WALL` → false
- FOV 射线穿过 Door，等同于穿过 FLOOR

```
blocks_sight(tile):
  WALL → true (阻挡)
  FLOOR → false (穿过)
  DOOR → false (穿过)  ← 无需修改 blocks_sight()
  STAIRS_DOWN → false
  LAVA → false
```

### 4.5 对 Save / AI / Combat / Hazard 的影响

| 系统 | 影响 | 原因 |
|------|------|------|
| Save/Load | 无 | 存档基于 seed 重新生成，不存 tile 数据 |
| AI 寻路 | 无 | is_walkable=true，AI 正常通过 |
| Combat | 无 | Door 不影响战斗逻辑 |
| Hazard | 无 | LAVA 是独立检测，Door 无交互 |
| Monster spawn | 无 | 在 Room interior 生成 |
| Player spawn | 无 | 在 Room[0] center 生成 |

---

## 5. Corridor 宽度评估

### 5.1 当前宽度

```
DUNGEON_CORRIDOR_MIN = 1
DUNGEON_CORRIDOR_MAX = 2

diamond radius 1:  3 tiles wide (96px)  — 十字形
diamond radius 2:  5 tiles wide (160px) — 菱形
```

### 5.2 视觉对比

```
radius=1 (当前范围):        radius=2 (当前范围):
    #                            #
   ###                         #####
    #                        #######
   ###                         #####
    #                            #

宽度: 3 tiles (96px)        宽度: 5 tiles (160px)
占地图宽度: 2.5%            占地图宽度: 12.5%
像走廊 ✓                     像小房间 ✗
```

### 5.3 对比分析

| 指标 | radius=1 (3 tiles) | radius=2 (5 tiles) |
|------|---------------------|---------------------|
| 视觉宽度 | 96px — 窄走廊 | 160px — 宽走廊 |
| 占地图% | 2.5% | 12.5% |
| 与 Room 对比 | 明显比 Room 窄 | 接近小 Room 宽度 |
| 探索感 | 紧张、狭窄 | 开阔、轻松 |
| FOV 穿透 | 射线快速穿过 | 射线需多次穿透 |

### 5.4 建议

**默认 radius=1（3 tiles 宽）。** 理由：
1. 与 Room (5-8 tiles) 有明显宽度对比
2. 走廊视觉上"像走廊"，不像"开放空间"
3. 32px tile size 下 96px 宽度足够玩家行走
4. 与《以撒的结合》等经典肉鸽的走廊宽度一致

**保留 radius=2 作为可配置参数**，用于特殊场景（Boss 房连接、商店入口等）。

### 5.5 实现方式

```cpp
// config.h — 修改
inline constexpr int DUNGEON_CORRIDOR_WIDTH = 1;  // 默认 radius=1 (3 tiles wide)
```

走廊宽度在 `_carve_corridor` 中使用，不再随机 1-2，而是固定为配置值。特殊场景可覆盖。

---

## 6. 最终修改计划

### 6.1 文件变更清单

| 文件 | 变更内容 | 行数估算 |
|------|----------|----------|
| `game_map.h` | TileType +DOOR; Tile::door() | +3 |
| `game_map.cpp` | load_from_template 'D'; draw() DOOR 分支 | +10 |
| `dungeon_generator.h` | DoorPlacement; CorridorConnection; 新方法声明 | +18 |
| `dungeon_generator.cpp` | _pick_room_edge; _compute_door_pos; 修改 _connect_rooms; 修改 _build_template; 修改 _carve_diamond; 移除旧 _pick_room | +80 / -15 |
| `config.h` | DUNGEON_CORRIDOR_WIDTH | +1 |
| `tests/world/dungeon_topology_test.cpp` | 新建，10 个测试 | +150 |
| `tests/CMakeLists.txt` | 新增测试 | +1 |

**总计：+262 / -15 行**

### 6.2 实施步骤

```
Step 1: TileType::DOOR + Tile::door() + load_from_template 'D'
        验证: 编译通过, 36/36 测试通过

Step 2: DungeonGenerator 新增结构体 (DoorPlacement, CorridorConnection)
        验证: 编译通过

Step 3: _carve_diamond 添加 '#' 条件检查
        验证: 现有测试通过 (不影响当前生成)

Step 4: 实现 _pick_room_edge + _compute_door_pos
        验证: 单元测试 Edge/Door 位置

Step 5: 修改 _connect_rooms + _build_template + _carve_corridor
        验证: 单元测试 AllRoomsReachable, CorridorNotInRoom, DoorNotFloating

Step 6: 渲染 DOOR tile
        验证: 实机确认视觉

Step 7: 全量测试 + world_validator + 实机 F1-F7
        验证: 46/46 测试通过
```

### 6.3 新增测试清单

| # | 测试名 | 验证内容 |
|---|--------|----------|
| 1 | `DoorTile_Walkable` | Tile::door().is_walkable == true |
| 2 | `DoorTile_NotWall` | Tile::door().type == TileType::DOOR |
| 3 | `BlocksSight_DoorFalse` | blocks_sight(DOOR) == false |
| 4 | `LoadTemplate_Door` | load_from_template 正确处理 'D' |
| 5 | `PickRoomEdge_Closest` | _pick_room_edge 选择最近边缘 |
| 6 | `ComputeDoorPos_OutsideRoom` | Door 在 Room 外部 1 格 |
| 7 | `Door_NotAtMapBoundary` | Door 不在 (0,0)-(39,29) 边界上 |
| 8 | `Corridor_NotInRoom` | 走廊 tile 不在任何 Room interior 内 |
| 9 | `Door_BetweenRoomAndCorridor` | Door 一侧是 Room floor，另一侧是 Corridor |
| 10 | `AllRoomsReachable` | 从 Room[0] BFS 可达所有 Room |

### 6.4 回归风险

| 风险 | 等级 | 缓解措施 |
|------|------|----------|
| 走廊 diamond 破坏 Room interior | 高 | `_carve_diamond` 添加 '#' 条件 |
| Door 越界 | 中 | `_pick_room_edge` 边界过滤 |
| BSP 树结构变化 | 低 | 分割逻辑不变 |
| 渲染性能 | 低 | 多一个 if-else 分支 |
| 测试覆盖不足 | 中 | 10 个专项测试 |

---

## 7. Door 未来扩展接口

Phase 2 实现静态 Door (OPEN)，但接口设计支持未来升级：

```cpp
// 预留接口（Phase 2 不实现）
enum class DoorState { OPEN, CLOSED, LOCKED, SEALED };

struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;
    bool is_visible = false;
    bool is_explored = false;
    // Phase 4+ 扩展:
    // DoorState door_state = DoorState::OPEN;
};

// Phase 4+ 的 blocks_sight 需要修改:
// bool blocks_sight(int x, int y) const {
//     if (type == TileType::DOOR)
//         return door_state != DoorState::OPEN;
//     return type == TileType::WALL;
// }
```

**Phase 2 的 Door 行为等同于 FLOOR**，但保留了未来升级为动态 Door 的扩展点。
