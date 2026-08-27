# Phase 2: Dungeon Topology — Room Boundary + Door + Enclosed Corridor

> 状态：设计方案（待审核）
> 前置：Phase 1 FOV ✅
> 目标：`Room → Door → Corridor → Door → Room` 封闭空间结构
> 约束：不引入动态 Door 状态 (OPEN/CLOSED/LOCKED)，Phase 2 的 Door 默认开启

---

## 1. 当前问题定位

### 1.1 走廊连接点（精确位置）

```
dungeon_generator.cpp:184-194 — _connect_rooms()
dungeon_generator.cpp:196-206 — _pick_room()
```

**当前算法：**
```
_connect_rooms(node):
    a = _pick_room(node->left)   → 返回 room_center()
    b = _pick_room(node->right)  → 返回 room_center()
    _corridors.emplace_back(a.x, a.y, b.x, b.y)
```

**问题：** `_pick_room` 返回 `room_center()`（房间几何中心）。走廊从房间中心雕刻到另一个房间中心，穿过房间内部空间和墙壁。

### 1.2 走廊雕刻方式

```
dungeon_generator.cpp:223-232 — _carve_corridor()
dungeon_generator.cpp:234-244 — _carve_line()
dungeon_generator.cpp:246-253 — _carve_diamond()
```

走廊 = L 形路径 + diamond 半径 1-2。路径从 Room Center 到 Room Center，无 Door 概念。

### 1.3 模板生成

```
dungeon_generator.cpp:208-215 — _build_template()
```

全部 '#' → '.' 雕刻。无 Door tile。Room 和 Corridor 的 '.' 完全相同。

---

## 2. Door Tile 设计

### 2.1 TileType 新增

```cpp
// game_map.h
enum class TileType { FLOOR, WALL, STAIRS_DOWN, LAVA, DOOR };
```

### 2.2 Door Tile 属性

| 属性 | 值 | 理由 |
|------|-----|------|
| `type` | `TileType::DOOR` | 识别用 |
| `is_walkable` | `true` | 玩家可以通过 |
| `blocks_sight` | `false` | Door 默认开启，不阻挡视线 |
| 渲染 | 独立颜色/标记 | 区别于 FLOOR 和 WALL |

### 2.3 blocks_sight() 修改

```cpp
// game_map.cpp:82-87
bool GameMap::blocks_sight(int x, int y) const {
    if (!_in_bounds(x, y)) return true;
    return _tiles[y][x].type == TileType::WALL;
    // DOOR: blocks_sight = false (默认开启)
    // 无需修改 — DOOR != WALL，自然返回 false
}
```

**结论：`blocks_sight()` 无需修改。** DOOR type 不是 WALL，自然不阻挡视线。

### 2.4 FOV 兼容性

FOV 射线穿过 DOOR tile 时：
- `blocks_sight(DOOR)` = false → 射线继续
- `is_explored` 被设为 true
- `is_visible` 被设为 true（当前帧在 FOV 内时）

**FOV 系统无需修改。** Door 默认开启，行为等同于 FLOOR。

### 2.5 渲染设计

在 `GameMap::draw()` 的 tile type 分支中新增 DOOR 渲染：

```
DOOR: 
  - 基色: 棕色系 {140, 100, 50, 255}（木质门）
  - 边框: 深棕 {100, 70, 30, 255}
  - 小标记: 门把手点 {200, 180, 100, 255}
  - 亮度: 与 FLOOR 相同的 bright 因子（FOV 已探索/可见）
```

---

## 3. DungeonGenerator 修改方案

### 3.1 新增数据结构

```cpp
// dungeon_generator.h — 新增
struct DoorPlacement {
    int x, y;           // Door tile 坐标
    int room_index;     // 所属 Room 索引
};

// DungeonGenerator 新增成员
std::vector<DoorPlacement> _doors;
```

### 3.2 修改 _pick_room → _pick_room_edge

**废弃 `_pick_room`（返回 room center），替换为 `_pick_room_edge`（返回房间边缘连接点）。**

```
_pick_room_edge(BSPNode* node, int target_x, int target_y) → (edge_x, edge_y)

逻辑：
1. 获取 node 的房间矩形 (rx, ry, rw, rh)
2. 计算房间四条边的中点：
   - 上边中点: (rx + rw/2, ry)
   - 下边中点: (rx + rw/2, ry + rh - 1)
   - 左边中点: (rx, ry + rh/2)
   - 右边中点: (rx + rw - 1, ry + rh/2)
3. 选择距离 (target_x, target_y) 最近的边缘中点
4. 返回该边缘中点坐标
```

**为什么用边缘中点而不是随机边缘点：**
- 中点保证走廊从房间的"正式入口"进入
- 避免走廊从角落进入（角落连接视觉效果差）
- 简化实现，不需要碰撞检测

### 3.3 修改 _connect_rooms

```
_connect_rooms(BSPNode* node):
    // 原有：递归处理子节点
    if (node->is_leaf()) return;
    _connect_rooms(node->left);
    _connect_rooms(node->right);
    if (!node->left || !node->right) return;

    // 新增：获取两个子树的房间边缘点
    auto room_a = _get_random_room(node->left);
    auto room_b = _get_random_room(node->right);
    
    // 选择连接点：Room A 的边缘 → Room B 的边缘
    auto edge_a = _pick_room_edge(node->left, room_b.cx, room_b.cy);
    auto edge_b = _pick_room_edge(node->right, room_a.cx, room_a.cy);
    
    // 计算 Door 位置：边缘点向外 1 格
    auto door_a = _compute_door_pos(edge_a, /*direction toward room_b*/);
    auto door_b = _compute_door_pos(edge_b, /*direction toward room_a*/);
    
    // 存储连接信息
    _corridor_connections.push_back({edge_a, door_a, door_b, edge_b});
```

### 3.4 新增 _compute_door_pos

```
_compute_door_pos(edge_point, direction) → (door_x, door_y)

逻辑：
- Door 放在 edge_point 向走廊方向 1 格的位置
- 即：edge_point 是房间墙壁上的点，Door 是墙壁外侧紧邻的 tile
- 确保 Door tile 是 '#'（wall），然后在模板中替换为 'D'
```

**等等——edge_point 本身就是房间边缘。** 房间内部是 '.'，房间边缘（墙壁）是 '#'。所以：

- `_pick_room_edge` 返回的应该是房间**内部**的边缘 tile（紧邻墙壁的地板 tile）
- Door 放在房间**外部**的墙壁 tile 上（紧邻房间的 '#' tile）

让我重新定义：

```
_pick_room_edge(node, target_x, target_y) → (floor_x, floor_y)

返回房间内部紧邻墙壁的地板 tile（即房间边缘的 '.' tile）。
选择距离 target 最近的方向。

示例：房间 (rx=5, ry=5, rw=6, rh=5)
  上边内部: (5+3, 5)   = (8, 5)   — 房间内第5行，紧邻上墙壁
  下边内部: (5+3, 5+4) = (8, 9)   — 房间内第9行，紧邻下墙壁
  左边内部: (5,   5+2) = (5, 7)   — 房间内第5列，紧邻左墙壁
  右边内部: (5+5, 5+2) = (10, 7)  — 房间内第10列，紧邻右墙壁
```

Door 放在 edge_point 向外 1 格的位置（即墙壁 tile 上）。

### 3.5 修改 _build_template

```
_build_template():
    // 原有：初始化全 '#'
    grid = 全 '#'
    
    // 原有：雕刻房间
    for room in _rooms: _carve_rect(grid, room)
    
    // 修改：雕刻走廊（从 edge 到 edge，不穿过房间内部）
    for conn in _corridor_connections:
        _carve_corridor(grid, conn.door_a, conn.door_b)
    
    // 新增：放置 Door
    for conn in _corridor_connections:
        grid[conn.door_a.y][conn.door_a.x] = 'D'
        grid[conn.door_b.y][conn.door_b.x] = 'D'
```

### 3.6 修改 _carve_corridor

**走廊不再连接 Room Center，而是连接 Door Position 到 Door Position。**

```
_carve_corridor(grid, door_a_x, door_a_y, door_b_x, door_b_y):
    // 走廊从 Door A 外侧开始，到 Door B 外侧结束
    // L 形路径，宽度 1-2（保持现有 diamond carving）
    // 走廊不进入房间内部
```

走廊路径：`door_a → ... 走廊空间 ... → door_b`

走廊不会穿过房间，因为 door_a 和 door_b 都在房间外部（墙壁外侧的 '#' tile 上）。

### 3.7 load_from_template 修改

```cpp
// game_map.cpp
void GameMap::load_from_template(const std::vector<std::string>& tmpl) {
    for (int y = 0; y < min(tmpl.size(), height); y++) {
        const auto& line = tmpl[y];
        for (int x = 0; x < min(line.size(), width); x++) {
            if (line[x] == '#')      _tiles[y][x] = Tile::wall();
            else if (line[x] == '.') _tiles[y][x] = Tile::floor();
            else if (line[x] == 'D') _tiles[y][x] = Tile::door();  // 新增
        }
    }
}
```

### 3.8 Tile::door() 工厂方法

```cpp
// game_map.h
struct Tile {
    // ... 现有 ...
    static Tile door() { return {TileType::DOOR, true, false, false}; }
};
```

---

## 4. _corridors 数据结构修改

**当前：** `_corridors` 是 `vector<tuple<int,int,int,int>>` — 存储 (x1,y1,x2,y2) 即 Room Center 坐标。

**改为：** `_corridor_connections` 存储完整的连接信息：

```cpp
struct CorridorConnection {
    std::pair<int,int> room_a_edge;   // Room A 内部边缘点 (floor tile)
    std::pair<int,int> door_a;        // Room A 外部 Door 位置 (wall tile → 'D')
    std::pair<int,int> door_b;        // Room B 外部 Door 位置 (wall tile → 'D')
    std::pair<int,int> room_b_edge;   // Room B 内部边缘点 (floor tile)
};
std::vector<CorridorConnection> _corridor_connections;
```

**旧的 `_corridors` 可以保留用于兼容，但 _build_template 改为使用 `_corridor_connections`。**

---

## 5. 影响分析

### 5.1 需要修改的文件

| 文件 | 修改内容 | 侵入度 |
|------|----------|--------|
| `game_map.h` | TileType 新增 DOOR; Tile::door() 工厂; 渲染分支 | 低 |
| `game_map.cpp` | load_from_template 处理 'D'; draw() 渲染 DOOR | 低 |
| `dungeon_generator.h` | DoorPlacement 结构; CorridorConnection 结构; 新方法声明 | 中 |
| `dungeon_generator.cpp` | _pick_room_edge; _compute_door_pos; 修改 _connect_rooms; 修改 _build_template; 修改 _carve_corridor | **高** |
| `config.h` | DUNGEON_CORRIDOR_WIDTH 新增 (可选) | 低 |

### 5.2 不需要修改的系统

| 系统 | 原因 |
|------|------|
| FOV | DOOR blocks_sight=false，射线穿过，无影响 |
| Save/Load | 存档基于 seed 重新生成，不存 tile 数据 |
| AI/寻路 | is_walkable=true，AI 可以正常通过 Door |
| Combat | 无直接影响 |
| Hazard | LAVA 是独立逻辑，Door 无交互 |
| Monster spawn | 在 Room 内部生成，Door 不影响 |
| Player spawn | 在 Room[0] 中心生成，Door 不影响 |

### 5.3 回归风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| 走廊与房间重叠 | 中 | Door 在房间外部，走廊从 Door 开始，不进入房间 |
| Door 被其他系统误用 | 低 | Door 仅在 _build_template 中生成 |
| BSP 树结构变化 | 低 | BSP 分割逻辑不变，只改变连接点选择 |
| 渲染性能 | 低 | 只多一个 tile type 分支 |
| 测试覆盖不足 | 中 | 需新增 dungeon topology 测试 |

---

## 6. 测试计划

### 6.1 新增测试

| 测试 | 验证内容 |
|------|----------|
| `DoorTile_Walkable` | Tile::door() 的 is_walkable == true |
| `DoorTile_NotWall` | Tile::door() 的 type == TileType::DOOR |
| `LoadTemplate_Door` | load_from_template 正确处理 'D' |
| `BlocksSight_DoorFalse` | blocks_sight(DOOR) == false |
| `RoomEdge_Top` | _pick_room_edge 选择上边缘中点 |
| `RoomEdge_Closest` | _pick_room_edge 选择最近的边缘 |
| `Corridor_NotInRoom` | 走廊 tile 不在任何 Room 矩形内 |
| `Door_BetweenRoomAndCorridor` | Door tile 在 Room 边缘和走廊之间 |
| `AllRoomsReachable` | 从 Room[0] BFS 可达所有 Room |
| `NoRoom穿墙` | Room 内部没有 Wall tile (除了边界) |

### 6.2 现有测试回归

- 36/36 现有测试必须继续通过
- 新增 10 个测试 → 总计 46 个

---

## 7. 实施顺序

```
Step 1: TileType::DOOR + Tile::door() + load_from_template 'D'
        ↓ 验证: 编译通过, 现有测试通过
Step 2: DungeonGenerator 新增 DoorPlacement + CorridorConnection 结构
        ↓ 验证: 编译通过
Step 3: 实现 _pick_room_edge + _compute_door_pos
        ↓ 验证: 单元测试 RoomEdge_*
Step 4: 修改 _connect_rooms 使用新连接算法
        ↓ 验证: 单元测试 Corridor_NotInRoom, Door_BetweenRoomAndCorridor
Step 5: 修改 _build_template 放置 Door + 走廊从 Door 到 Door
        ↓ 验证: 单元测试 AllRoomsReachable, NoRoom穿墙
Step 6: 渲染 DOOR tile (draw() 新增分支)
        ↓ 验证: 实机确认 Door 视觉
Step 7: 全量测试 + world_validator + 实机 F1-F7
        ↓ 验证: 46/46 测试通过, 地图体验确认
```

---

## 8. Minimap 设计 (Phase 3，本阶段不实现)

**原则：**
- 复用 `Tile::is_explored` / `Tile::is_visible`
- 不维护第二套探索状态
- 未探索 → 不显示
- 已探索 → 永久痕迹（灰暗）
- 当前可见 → 正常亮度
- 玩家位置 → 明确标记

**实现时机：** Phase 2 验证通过后

---

## 9. 未来演进路线

```
Phase 2 (当前):
  Door = 静态开启状态, walkable=true, blocks_sight=false
  ↓
Phase 3: Minimap
  ↓
Phase 4+: Dynamic Door
  DoorState: OPEN / CLOSED / LOCKED / SEALED
  - 进入战斗 → 门关闭
  - 清空房间 → 门打开
  - Boss 房 → 特殊门
  - 商店 → 特殊入口
  - 隐藏房 → 墙壁可炸开
```
