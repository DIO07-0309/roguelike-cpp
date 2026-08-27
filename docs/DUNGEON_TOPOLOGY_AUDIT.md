# Dungeon Topology / Connectivity Audit

> Phase: Pre-design audit (只读分析，未修改任何代码)
> Date: 2026-08-27

---

## 1. 地图生成全流程

```
generate()
  ├─ _partition(root)          BSP 递归分割 40×30 → 多个区域
  ├─ _create_rooms(root)       每个叶子节点内随机放置一个矩形房间
  ├─ _connect_rooms(root)      每对兄弟叶子之间 L 形走廊连接
  ├─ _build_template()         char grid: '#'=墙, '.'=地板
  │   ├─ 初始化: 全 '#'
  │   ├─ _carve_rect(): 每个房间 → '.'
  │   └─ _carve_corridor(): 每条走廊 → '.'
  └─ gm->load_from_template()  '#' → Tile::wall(), '.' → Tile::floor()
```

---

## 2. Tile 默认值

| 问题 | 答案 |
|------|------|
| 地图初始 Tile 是什么？ | **全部 WALL** — `_init_walls()` 将所有 40×30=1200 个 Tile 设为 `Tile::wall()` (type=WALL, is_walkable=false) |
| Room 外部 Tile 是否 walkable？ | **否** — Room 外部保持为 WALL (walkable=false)，除非走廊雕刻穿过了它 |
| 走廊 Tile 是否 walkable？ | **是** — 走廊雕刻将 '#' 替换为 '.' → `Tile::floor()` (walkable=true) |

**结论：不存在"整个地图默认可走"的问题。** 默认全部是墙，只有 Room 和 Corridor 区域被雕刻成地板。

---

## 3. Room 放置逻辑

```cpp
// dungeon_generator.cpp:168-182
void DungeonGenerator::_create_rooms(BSPNode* node) {
    // 仅在叶子节点创建房间
    int rw = _min_room + _rand_int(max(1, node->w - 2*_margin - _min_room + 1));  // 5 ~ node.w-2
    int rh = _min_room + _rand_int(max(1, node->h - 2*_margin - _min_room + 1));  // 5 ~ node.h-2
    int rx = node->x + _margin + _rand_int(max(1, node->w - rw - 2*_margin + 1));
    int ry = node->y + _margin + _rand_int(max(1, node->h - rh - 2*_margin + 1));
    _rooms.emplace_back(rx, ry, rw, rh);
}
```

**关键参数：**
- `_min_part = 8` (BSP 最小分区)
- `_min_room = 5` (最小房间)
- `_margin = 1` (房间距分区边界的间距)

**Room = 矩形 '.' 区域。** 没有显式的"房间边界"或"墙壁"——房间就是一块地板，被周围的 '#' 墙包围。

---

## 4. 走廊生成逻辑

```cpp
// dungeon_generator.cpp:223-232
void DungeonGenerator::_carve_corridor(grid, x1, y1, x2, y2) {
    int width = 1 或 2 (diamond radius);
    // 随机选择 L 形方向:
    //   水平→垂直: carve(x1,y1 → x2,y1) + carve(x2,y1 → x2,y2)
    //   垂直→水平: carve(x1,y1 → x1,y2) + carve(x1,y2 → x2,y2)
}
```

**走廊 = L 形路径，宽度 1-2 格 (diamond radius)。**

`_carve_diamond` 在路径每个点雕刻一个菱形：
- radius=1: 5 个 tile (十字形)
- radius=2: 13 个 tile (菱形)

**走廊连接的是 Room CENTER（房间中心），不是 Room EDGE（房间边缘）。**

这意味着走廊从房间中心出发，穿过房间内部，穿过房间墙壁，经过房间之间的墙壁空间，再穿过目标房间的墙壁，到达目标房间中心。

---

## 5. Wall 生成逻辑

**没有显式的 wall 生成步骤。** Wall 就是 Room 和 Corridor 雕刻后**剩余的 '#' tile**。

```
初始: 全 '#'
雕刻 Room: 部分 '#' → '.'
雕刻 Corridor: 部分 '#' → '.'
剩余: '#' 保持为 WALL
```

---

## 6. 关键发现：为什么"房间感"不够

### 6.1 没有门 (Door)

当前系统中**没有 Door 概念**。走廊穿过房间墙壁时，只是将墙壁 tile 从 '#' 变成 '.'。没有特殊的 Door tile，没有视觉标记，没有逻辑门槛。

```
当前: [Room] [墙被走廊穿透 → 地板] [走廊] [墙被走廊穿透 → 地板] [Room]
                    ↑ 没有门              ↑ 没有门
```

### 6.2 走廊连接 Room CENTER，不连接 Room EDGE

```cpp
// _pick_room() 返回 room_center()
// _connect_rooms() 用 room_center() 作为走廊端点
```

这导致：
1. 走廊穿过房间内部空间（房间中心到房间墙壁）
2. 走廊的"出口"不是在房间边缘，而是在房间内部的某个点
3. 玩家从房间中心出发，走过房间地板，走过被穿透的墙壁，进入走廊——没有明确的"离开房间"时刻

### 6.3 走廊太窄

`DUNGEON_CORRIDOR_MIN=1, DUNGEON_CORRIDOR_MAX=2`

走廊宽度只有 1-2 个 tile (32-64 像素)。在视觉上几乎看不出是"走廊"——更像是墙上的一条裂缝。

### 6.4 Room 没有"边界墙"

Room 是一个矩形 '.' 区域。Room 的"墙壁"就是周围未被雕刻的 '#' tile。但这些 '#' tile 和其他地方的 '#' tile 完全一样——没有视觉区分，没有逻辑标记。

当走廊穿过 Room 的墙壁时，只是将几个 '#' tile 变成 '.' tile。没有保留"这是房间的入口"的信息。

### 6.5 没有 Room/Corridor 的类型区分

`load_from_template` 中：
- `'#'` → `Tile::wall()`
- `'.'` → `Tile::floor()`

所有地板都是同一种 Tile。没有 "room_floor" vs "corridor_floor" 的区分。没有视觉差异（颜色、纹理）。

---

## 7. 为什么地图感觉像"开放世界"

综合以上分析，"开放世界感"来自：

| 原因 | 影响 |
|------|------|
| 走廊连接 Room Center | 走廊穿过房间内部，房间没有明确的"出口" |
| 没有 Door tile | 房间和走廊之间没有过渡/门槛 |
| 走廊太窄 (1-2 tile) | 走廊几乎不可见，看起来像房间之间的空隙 |
| Room 墙壁只有 1 tile | 墙壁太薄，不像真实的房间边界 |
| 所有地板相同 | Room 和 Corridor 没有视觉区分 |

**最核心的问题：Room 不是封闭空间。** 走廊直接穿透房间墙壁，没有 Door 过渡，没有视觉标记，玩家感觉不到"我在房间里" vs "我在走廊上"的区别。

---

## 8. 是否存在"视觉是墙，但逻辑可行走"？

**不存在。** `load_from_template` 正确映射：
- `'#'` → `Tile::wall()` (type=WALL, walkable=false) ✓
- `'.'` → `Tile::floor()` (type=FLOOR, walkable=true) ✓

`set_tile` 也正确维护 `is_walkable`：
```cpp
void GameMap::set_tile(int x, int y, TileType t) {
    _tiles[y][x].type = t;
    _tiles[y][x].is_walkable = (t != TileType::WALL);
}
```

---

## 9. 最小修改方案设计方向 (待讨论)

### 方案 A: Door Tile (最小侵入)

**目标：** 在 Room 和 Corridor 之间插入 Door tile。

1. 新增 `TileType::DOOR` (walkable=true)
2. 修改 `_connect_rooms`：找到走廊与房间墙壁的交点，将该 tile 设为 DOOR
3. `draw()` 中 Door 用不同颜色/纹理渲染
4. FOV: Door 阻挡视线 (`blocks_sight` 对 DOOR 返回 true 或 false 取决于设计)

**优点：** 最小改动，只加一个 tile 类型
**缺点：** 房间本身仍然不是封闭的（走廊仍然穿透墙壁）

### 方案 B: Room Wall + Door (中等侵入)

**目标：** 房间有显式的墙壁边界，走廊通过 Door 连接。

1. 房间生成时，在房间外围添加一圈 WALL tile（即使原来就是 WALL，也标记为"房间边界"）
2. 走廊与房间边界相交时，在交点放置 DOOR tile
3. 走廊本身也有墙壁边界（两侧各一排 WALL）

**优点：** 房间真正封闭，有明确的 Door
**缺点：** 需要修改 Room 生成和 Corridor 生成逻辑

### 方案 C: Room Corridor Separation (最大侵入)

**目标：** Room 和 Corridor 是完全独立的空间类型。

1. 新增 `TileType::CORRIDOR_FLOOR` (walkable=true, 视觉不同于 Room)
2. Room 保持现有生成方式
3. Corridor 改为：先生成路径，再在路径两侧生成 WALL，最后在路径中心生成 CORRIDOR_FLOOR
4. Corridor 与 Room 的连接点放置 DOOR

**优点：** 最清晰的拓扑结构，Room 和 Corridor 完全分离
**缺点：** 改动最大，需要重写 Corridor 生成逻辑

---

## 10. Minimap 规划 (暂不实现)

**原则：**
- 复用 `Tile::is_visible` / `Tile::is_explored`，不维护第二套状态
- 未探索: 完全不显示
- 已探索但不可见: 灰暗显示
- 当前可见: 正常亮度

**实现时机：** Room/Corridor 拓扑稳定后再实现
