# Door Interaction & Room Visibility Audit

> **目的**：审计当前门交互与视野系统的根因问题，提出最小改动方案。
> **状态**：设计文档，待审批后实施。**不编码。**
> **日期**：2026-08-28

---

## 目录

1. [问题 1 根因：门交互卡顿](#1-问题-1-根因门交互卡顿)
2. [问题 2 根因：FOV 跨房间穿透](#2-问题-2-根因fov-跨房间穿透)
3. [E 键自动过门：最小架构方案](#3-e-键自动过门最小架构方案)
4. [门状态机设计](#4-门状态机设计)
5. [房间视野控制方案](#5-房间视野控制方案)
6. [系统影响矩阵](#6-系统影响矩阵)
7. [最小实施批次](#7-最小实施批次)
8. [测试计划](#8-测试计划)

---

## 1. 问题 1 根因：门交互卡顿

### 1.1 现状分析

当前门交互流程（`player_controller.cpp:131-150`）：

```
玩家按方向键 → position += move * speed * dt
  → is_rect_walkable() 检测碰撞
  → 撞墙/撞 CLOSED 门 → try_open_door_toward()
  → 如果是 CLOSED 门 → 门变 OPEN → 重试移动
```

**问题：这个流程有两个严重缺陷。**

### 1.2 缺陷 A：撞 OPEN 门 = 撞墙（同一帧双重位移）

```cpp
// player_controller.cpp:131-150
e.position.x += move.x * s; e.sync_rect();           // 1. 先移动
if (!gs.game_map->is_rect_walkable(e.rect)) {        // 2. 检查
    if (gs.game_map->try_open_door_toward(...)) {    // 3a. 是 CLOSED 门 → 开门+重试
        e.position.x += move.x * s; e.sync_rect();   //    总位移 = 2x
    } else {
        e.position.x -= move.x * s; e.sync_rect();   // 3b. 是墙 → 回退
    }
}
```

**当玩家站在门格子上（OPEN），向走廊方向移动时：**
- `is_rect_walkable()` 采样 8 个点，只要有一个点落在墙格子上就返回 false
- 门格子本身是 walkable=true，但门两侧紧邻墙壁
- 由于实体 32×32 = 1 格，8 采样点中有 4 个角点 + 4 个边中点，任何一个碰到相邻墙格就会判定为"撞墙"
- **结果**：玩家站在 OPEN 门上向走廊移动时，经常被误判为"撞墙"，被弹回门格子中央

**这直接解释了"门卡住"现象：玩家过了 OPEN 门想走出去，但门格子两侧的墙壁导致碰撞检测失败，玩家被反复弹回。**

### 1.3 缺陷 B：try_open_door_toward 只检测 1 格方向

```cpp
// game_map.cpp:119-134
auto [cx, cy] = pixel_to_tile(r.x + r.width / 2, r.y + r.height / 2);
int tx = cx + step_x, ty = cy + step_y;  // 只看中心 tile 往方向偏 1 格
```

**场景**：玩家在房间内，距离门 2 格以上，按方向键移动。
- `try_open_door_toward` 只看中心 tile 往方向偏 1 格的位置
- 如果那个位置不是 DOOR tile，就返回 false → 玩家被弹回
- **玩家必须紧贴门格子才能触发开门**

### 1.4 缺陷 C：开门 = 同帧通过，无动画

当成功触发 `try_open_door_toward` 时：
1. 门立即变 OPEN
2. 移动重试（`e.position.x += move.x * s`）
3. 同一帧内，玩家已经穿过了门

**没有开门动画、没有停顿、没有"按下 E 键开门"的感觉。** 玩家甚至不知道自己开了门。

### 1.5 缺陷 D：8 点碰撞采样过于严格

```cpp
// game_map.cpp:57-69 — is_rect_walkable 采样 8 个点
float pts[8][2] = {
    {r.x, r.y},                    // 左上
    {r.x + r.width - 1, r.y},      // 右上
    {r.x, r.y + r.height - 1},     // 左下
    {r.x + r.width - 1, r.y + r.height - 1},  // 右下
    {r.x + r.width/2, r.y},        // 上中
    {r.x + r.width/2, r.y + r.height - 1},    // 下中
    {r.x, r.y + r.height/2},       // 左中
    {r.x + r.width - 1, r.y + r.height/2}     // 右中
};
```

**门格子（32×32）位于两面墙之间，通道宽度仅 1 格（32px）。**
- 玩家实体也是 32×32
- 当玩家在门格子上向走廊移动 1px，4 个角点中可能有 1-2 个落在相邻墙壁上
- 8 点中只要有 1 个点落在墙上 → is_rect_walkable = false → 撞墙
- **门是 1 格宽，玩家是 1 格宽，但 8 点采样让"精确通过 1 格宽通道"变得不可能**

---

## 2. 问题 2 根因：FOV 跨房间穿透

### 2.1 现状分析

当前 FOV 系统（`game_map.cpp:164-187`）：

```cpp
void GameMap::update_fov(int cx, int cy, int radius) {
    for (int deg = 0; deg < 360; deg++) {
        float dx = cosf(deg * DEG2RAD);
        float dy = sinf(deg * DEG2RAD);
        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + roundf(dx * dist);
            int ty = cy + roundf(dy * dist);
            if (!_in_bounds(tx, ty)) break;
            _tiles[ty][tx].is_visible = true;
            _tiles[ty][tx].is_explored = true;
            if (blocks_sight(tx, ty)) break;
        }
    }
}
```

### 2.2 根因：OPEN 门 = 完全透明

```cpp
bool GameMap::blocks_sight(int x, int y) const {
    if (t.type == TileType::WALL) return true;
    if (t.type == TileType::DOOR) return t.door_state == DoorState::CLOSED;
    return false;
}
```

**OPEN 门的 `blocks_sight() = false`。**

这意味着：
1. 玩家站在 A 房间，看向连接走廊的 OPEN 门
2. FOV 射线穿过门（不被阻断）
3. 射线继续穿过走廊
4. 射线进入 B 房间（走廊连接的另一个房间）
5. B 房间所有 FLOOR 瓦片都被标记为 `is_visible = true`
6. **玩家站在 A 房间，直接看到了整个 B 房间**

### 2.3 为什么房间内部也是完全可见的？

房间内部 = FLOOR 瓦片。`blocks_sight()` 对 FLOOR 返回 false。
所以一旦射线穿过门进入房间，射线会一直穿透到半径尽头或撞墙才停。

**整个房间 + 连接走廊 = 一次性全暴露。**

### 2.4 玩家期望 vs 现状

| 场景 | 玩家期望 | 实际情况 |
|:---|:---|:---|
| 站在门口看 OPEN 门 | 看到走廊一小段 | 看穿整个走廊 + 连通房间 |
| 站在走廊看两个 OPEN 门 | 看到两个门洞 | 看穿两个房间 |
| 通过 OPEN 门进入新房间 | 进入时逐渐发现 | 在走廊就已全亮 |

### 2.5 为什么"记忆态"也不够好

当前系统：房间被探索后 → `is_explored = true` → 渲染为 60% 暗。

**但这只在"离开房间"后才有区别。** 在玩家站在门旁边时：
- 当前可见（`is_visible = true`）= 全亮 100%
- 射线直接穿透到整个房间
- **在进入房间之前，整个房间就是亮的，不存在"逐渐发现"**

---

## 3. E 键自动过门：最小架构方案

### 3.1 设计原则

1. **OPEN 门 = 不需要交互，直接走过**（现有行为 OK，但需要修碰撞）
2. **CLOSED 门 = 靠近后按 E 键开门**（新增交互）
3. **LOCKED 门 = 按 E 键触发条件检查**（Batch 3 扩展）
4. **开门后自动前进一格**（不需要再按方向键）

### 3.2 交互状态机

```
           ┌─────────────────────────────┐
           │      DoorState 枚举          │
           │                             │
           │  NONE ──→ OPEN ──→ CLOSED   │
           │             ↑        │       │
           │             └────────┘       │
           │           (E 键开门)         │
           │                             │
           │  LOCKED ──→ SEALED          │
           │  (Batch 3)  (Batch 3)       │
           └─────────────────────────────┘

  状态语义:
  ┌──────────┬──────────┬──────────┬──────────────────┐
  │  State   │ walkable │ blocks   │ 交互              │
  │          │          │ _sight   │                  │
  ├──────────┼──────────┼──────────┼──────────────────┤
  │  OPEN    │  true    │  false   │ 直接走过          │
  │  CLOSED  │  false   │  true    │ E 键开门→OPEN    │
  │  LOCKED  │  false   │  true    │ E 键→条件检查    │
  │  SEALED  │  false   │  true    │ 不可交互          │
  └──────────┴──────────┴──────────┴──────────────────┘
```

### 3.3 E 键自动过门流程

```
玩家按 E 键
  → 检测玩家中心 tile 4 方向邻格
  → 找到 CLOSED/LOCKED 门？
     ├─ YES → set_door_state(OPEN) + FOV 重算 + 自动前进 1 格到门格子
     └─ NO → 走现有 NPC/物品/楼梯/房间交互逻辑
```

### 3.4 最小改动清单

| 改动 | 文件 | 内容 | 行数 |
|:---|:---|:---|:---|
| **E 键门交互** | `player_controller.cpp` | `handle_input()` 的 E 键分支新增：检测 4 方向邻格 CLOSED 门 → `set_door_state(OPEN)` + `on_door_opened()` + 自动位移到门格子 | ~20 行 |
| **移除自动撞门开门** | `player_controller.cpp` | 删除 `tick()` 中 X/Y 轴的 `try_open_door_toward()` 调用（E 键取代自动撞门） | ~10 行（删除） |
| **移除 Sim 门旁路** | `sim_ai.cpp` | 删除 `_tile_rect_walkable` 中 CLOSED 门的 `return true` 短路（Sim 也需要走 E 键逻辑，或改为 Sim 自动开门） | ~3 行 |
| **门格子碰撞修复** | `game_map.cpp` | `is_rect_walkable` 对 `TileType::DOOR` + `OPEN` 做特殊处理：门格子中心点始终 walkable（或减少采样到 4 角点） | ~5 行 |

**总计：~35 行改动（含删除）。**

### 3.5 Sim AI 适配方案

Sim 不能按 E 键。两个方案：

**方案 A（推荐）**：Sim DecisionAgent 在 tick 中检测前方 CLOSED 门 → 自动调用 `set_door_state(OPEN)` + 位移。
- 优点：与玩家体验一致（都是"决定开门→开门→通过"）
- 缺点：需要在 sim_ai.cpp 加 ~10 行

**方案 B**：保留 `_tile_rect_walkable` 的 CLOSED 门旁路，但不修改玩家行为。
- 优点：零改动
- 缺点：Sim 与玩家行为不一致（Sim 自动穿门，玩家要按 E）

**建议采用方案 A。** Sim 的门交互应该是"自动开门"而不是"穿门"，这样语义正确。

---

## 4. 门状态机设计

### 4.1 当前状态

```
Tile::door_state: uint8_t { NONE = 0, OPEN = 1, CLOSED = 2 }
```

### 4.2 扩展后状态

```cpp
enum class DoorState : uint8_t {
    NONE   = 0,   // 非门瓦片
    OPEN   = 1,   // 开启（walkable, 不挡视线）
    CLOSED = 2,   // 关闭（不可走, 挡视线, E 键可开）
    LOCKED = 3,   // 锁住（不可走, 挡视线, 需要条件解锁）
    SEALED = 4    // 密封（不可走, 挡视线, 不可解锁）
};
```

### 4.3 状态转换规则

```
生成时:   所有门 → OPEN（D2 决策）

战斗时:   RoomManager._try_lock() → CLOSE 门 → LOCKED 场景下是 CLOSED
清房后:   RoomManager._try_unlock() → OPEN 门

E 键:     CLOSED → OPEN（自动前进）
          LOCKED → 检查条件 → 满足则 OPEN，否则提示

未来扩展:
          LOCKED → 需要钥匙/击败特定怪/解谜 → OPEN
          SEALED → 永不可开（路径封锁）
```

### 4.4 对现有代码的影响

`set_door_state()` 已经是通用的状态设置函数，只需扩展枚举即可。
`blocks_sight()` 已经正确处理：`DOOR` 类型只看 `door_state == CLOSED` 时挡视线。
→ **LOCKED/SEALED 也应挡视线，需要修改 `blocks_sight()` 为 `door_state != OPEN`。**

```cpp
// 修改前:
if (t.type == TileType::DOOR) return t.door_state == DoorState::CLOSED;

// 修改后:
if (t.type == TileType::DOOR) return t.door_state != DoorState::OPEN;
```

---

## 5. 房间视野控制方案

### 5.1 设计目标

> **玩家在一个房间内时，即使门是 OPEN，也不应该直接看到另一个完整房间。**
> **进入房间后才获得该房间正常视野。**

### 5.2 方案：房间遮罩 (Room Visibility Mask)

**核心思想**：在 FOV 射线传播过程中，增加"房间边界"阻断规则。

#### 5.2.1 什么时候射线应该被阻断？

当前阻断条件：`blocks_sight()` → WALL 或 CLOSED 门。

**新增阻断条件**：射线从一个房间穿过门进入另一个房间时，**只允许射线传播到门格子对面 1-2 格**，而不是直接穿透整个房间。

#### 5.2.2 实现方案：门格子视野截断

```
当前行为:
  射线 → 穿过 OPEN 门 → 继续传播 → 穿过整个 B 房间

修改后:
  射线 → 穿过 OPEN 门 → 到达门对面 1 格 → 停止
  （除非玩家当前也在 B 房间内）
```

**具体逻辑**：

```cpp
// 在 update_fov() 中，射线传播循环内新增：
if (t.type == TileType::DOOR && t.door_state == DoorState::OPEN) {
    // 射线穿过门，记录"已穿过门"
    passed_door = true;
    door_exit_x = tx + step_x;  // 门对面 1 格
    door_exit_y = ty + step_y;
    continue;  // 允许传播到门对面 1 格
}
if (passed_door && tx == door_exit_x && ty == door_exit_y) {
    // 到达门对面，检查：玩家是否在门对面的房间内？
    if (room_at(cx, cy) != room_at(tx, ty)) {
        break;  // 玩家不在目标房间 → 截断
    }
    passed_door = false;  // 玩家在同一房间 → 继续
}
```

#### 5.2.3 更简洁的方案：门格子作为"视野墙"

**最小改动方案**：OPEN 门在 FOV 中的行为改为"不挡视线但限制传播距离"。

```cpp
// 修改 blocks_sight() 语义 — 但这会改变"门是透明的"定义
// 不推荐
```

**推荐方案：在 update_fov() 中对门格子做特殊处理**

```cpp
void GameMap::update_fov(int cx, int cy, int radius) {
    // ... 清除 is_visible ...
    
    for (int deg = 0; deg < 360; deg++) {
        float dx = cosf(deg * DEG2RAD);
        float dy = sinf(deg * DEG2RAD);
        int last_door_x = -1, last_door_y = -1;  // 上一个穿过的门
        
        for (float dist = 0; dist <= radius; dist += 0.5f) {
            int tx = cx + roundf(dx * dist);
            int ty = cy + roundf(dy * dist);
            if (!_in_bounds(tx, ty)) break;
            
            _tiles[ty][tx].is_visible = true;
            _tiles[ty][tx].is_explored = true;
            
            // 门格子特殊处理
            if (_tiles[ty][tx].type == TileType::DOOR) {
                if (_tiles[ty][tx].door_state == DoorState::CLOSED) {
                    break;  // CLOSED 门 = 挡视线（现有行为）
                }
                // OPEN 门：记录位置，允许继续传播 1 格
                last_door_x = tx;
                last_door_y = ty;
                continue;
            }
            
            // 非门格子：如果上一步是门，检查是否在门对面 1 格
            if (last_door_x >= 0) {
                // 检查玩家是否在门对面的房间内
                int player_room = room_at(cx, cy);
                int target_room = room_at(tx, ty);
                if (player_room != target_room && player_room >= 0 && target_room >= 0) {
                    break;  // 跨房间截断
                }
                last_door_x = -1;  // 重置
            }
            
            if (blocks_sight(tx, ty)) break;
        }
    }
}
```

**这个方案的关键**：
1. OPEN 门本身不挡视线（射线可以穿过）
2. 穿过门后，射线只传播 1 格到门对面的格子
3. 如果那个格子属于另一个房间（玩家不在其中），射线停止
4. 如果玩家在同一个房间内（比如从走廊看同一走廊的另一段），射线继续

### 5.3 需要 room_at() 查询

`RoomManager::room_at(tx, ty)` 已经存在，返回 tile 所属的房间索引。
**但 FOV 在 `GameMap` 中，`RoomManager` 在 `RoomManager` 中。**

两个解法：

**方案 A（推荐）**：给 `GameMap` 增加一个 `int room_id_at(int tx, int ty)` 查询，由 `RoomManager::build()` 时一次性写入每个房间瓦片的 room_id。FOV 只查 `GameMap` 自己的数据。

**方案 B**：`update_fov()` 接收 `const RoomManager&` 参数。但这引入了 GameMap 对 RoomManager 的依赖。

**推荐方案 A。** 在 `GameMap` 中增加一个 `int _room_id[MAP_HEIGHT][MAP_WIDTH]` 数组（默认 -1），`RoomManager::build()` 时写入。FOV 用它做跨房间截断。

### 5.4 "进入房间后才获得正常视野"的效果

```
玩家在 A 房间（room_id = 0），门对面是 B 房间（room_id = 1）:

1. 射线从 A 房间出发
2. 穿过 A 房间的 OPEN 门（记录 last_door）
3. 到达门对面 1 格（走廊，room_id = -1）
4. player_room (0) != target_room (-1) → 但 -1 是走廊，不算房间
5. 继续传播（走廊是中性区域）
6. 射线到达 B 房间的门（又一个 OPEN 门，记录 last_door）
7. 到达 B 房间内部 1 格（room_id = 1）
8. player_room (0) != target_room (1) → 截断！

结果：玩家在 A 房间，只能看到走廊 + B 房间的门口 1 格，
      不能看到整个 B 房间。
```

**当玩家走进 B 房间后**：
- player_room = 1（B 房间）
- 射线从 B 房间出发
- 穿过门，到达 A 房间门口 1 格
- player_room (1) != target_room (0) → 截断
- **但玩家已经在 B 房间内，B 房间内部的射线全部正常传播**
- **结果：B 房间全亮，A 房间只看到门口**

### 5.5 走廊特殊处理

走廊（room_id = -1）不应该阻断射线。玩家在走廊中应该能看到走廊全段 + 两侧门口。

修改截断规则：
```cpp
if (player_room != target_room && player_room >= 0 && target_room >= 0) {
    break;  // 只有两边都属于不同房间时才截断
}
// 如果 target_room == -1（走廊），继续传播
```

---

## 6. 系统影响矩阵

| 系统 | 改动内容 | 影响程度 | 备注 |
|:---|:---|:---|:---|
| **PlayerController** | E 键分支新增门交互；删除自动撞门开门 | **中** | 核心交互改动 |
| **GameMap** | `is_walkable` 门格子特殊处理；`blocks_sight` 扩展 LOCKED/SEALED；新增 `_room_id` 数组；`update_fov()` 跨房间截断 | **高** | FOV + 碰撞核心改动 |
| **RoomManager** | `build()` 时写入 `_room_id` 到 GameMap | **低** | 1 行调用 |
| **FOV** | 射线传播增加门截断逻辑 | **中** | `update_fov()` 内 ~15 行 |
| **SimAI** | `_tile_walkable` 移除 CLOSED 旁路；DecisionAgent 新增自动开门逻辑 | **中** | Sim 行为对齐 |
| **Minimap** | `color_for_tile` 增加 DoorState 参数，OPEN/CLOSED/LOCKED 不同颜色 | **低** | ~5 行 |
| **DoorState** | 枚举增加 LOCKED=3, SEALED=4 | **低** | 枚举扩展 |
| **碰撞系统** | `is_rect_walkable` 门格子采样优化 | **低** | ~5 行 |

---

## 7. 最小实施批次

### Batch A：门交互重构（E 键 + 碰撞修复）

**目标**：玩家体验从"撞门穿门"变为"靠近按 E → 开门 → 自动前进"

| 步骤 | 改动 | 测试 |
|:---|:---|:---|
| A1 | `is_rect_walkable` 门格子中心点 walkable 修复 | 现有 door_interact_test |
| A2 | PlayerController 新增 E 键门交互（检测 4 方向 CLOSED 门 → 开门 + 位移） | 新增 door_ekey_test |
| A3 | PlayerController 删除自动撞门开门（`try_open_door_toward` 调用） | 现有 door_interact_test 改为 E 键触发 |
| A4 | SimAI DecisionAgent 新增自动开门（检测前方 CLOSED 门 → 开门） | sim 回归 |
| A5 | SimAI `_tile_walkable` 移除 CLOSED 旁路 | sim 回归 |

### Batch B：FOV 房间视野控制

**目标**：OPEN 门不再让射线穿透整个相邻房间

| 步骤 | 改动 | 测试 |
|:---|:---|:---|
| B1 | GameMap 新增 `_room_id` 数组 + `room_id_at()` 查询 | 单测 room_id 写入正确 |
| B2 | RoomManager::build() 写入 `_room_id` | 单测 room_id 覆盖所有房间瓦片 |
| B3 | `update_fov()` 增加门截断逻辑 | 新增 fov_room_visibility_test |
| B4 | SimAI FOV 适配（Sim 的 FOV 是否需要相同截断？） | sim 回归 |

### Batch C：门状态扩展 + Minimap

**目标**：LOCKED/SEALED 枚举 + 小地图差异化渲染

| 步骤 | 改动 | 测试 |
|:---|:---|:---|
| C1 | DoorState 枚举扩展 LOCKED=3, SEALED=4 | 编译通过 |
| C2 | `blocks_sight` 改为 `door_state != OPEN` | 现有 fov_test |
| C3 | Minimap `color_for_tile` 增加 DoorState 参数 | minimap_test 扩展 |
| C4 | Room Encounter 集成（LOCKED 状态触发条件解锁） | room_encounter_test 扩展 |

**建议执行顺序：Batch A → Batch B → Batch C**

Batch A 独立可交付（门交互体验立即改善）。Batch B 依赖 A（FOV 截断需要知道哪些是门）。Batch C 是锦上添花。

---

## 8. 测试计划

### Batch A 测试

| 测试 | 类型 | 内容 |
|:---|:---|:---|
| `door_ekey_test` | 新增 | 4 方向 E 键开门 + 自动前进 1 格 |
| `door_ekey_not_door` | 新增 | E 键在非门方向不触发开门 |
| `door_ekey_already_open` | 新增 | E 键对 OPEN 门不触发 |
| `door_ekey_closed_only` | 新增 | E 键只开 CLOSED 门，不开 LOCKED 门 |
| `door_walkability_fix` | 新增 | 站在 OPEN 门格子上可向任意方向移动 |
| `sim_door_auto_open` | 新增 | Sim AI 检测前方 CLOSED 门并自动开门 |
| 现有 door_interact_test | 改为 | 原"接触开门"测试改为"移动碰撞不再自动开门" |

### Batch B 测试

| 测试 | 类型 | 内容 |
|:---|:---|:---|
| `fov_room_visibility_test` | 新增 | 站在 A 房间，通过 OPEN 门，B 房间 tiles 不可见 |
| `fov_corridor_visibility` | 新增 | 站在走廊，走廊全段可见 + 两侧门口 1 格 |
| `fov_same_room_visibility` | 新增 | 站在房间内，房间内部全亮（不受截断影响） |
| `room_id_coverage_test` | 新增 | 所有房间瓦片 room_id 正确写入 |
| 现有 fov_test | 扩展 | 增加门截断场景 |

### Batch C 测试

| 测试 | 类型 | 内容 |
|:---|:---|:---|
| `door_state_extended_test` | 新增 | LOCKED/SEALED 枚举正确设置 |
| `minimap_door_colors_test` | 新增 | OPEN/CLOSED/LOCKED 三种颜色 |
| 现有 door_seal_test | 扩展 | LOCKED 状态下 blocks_sight = true |

### 回归测试

| 测试 | 内容 |
|:---|:---|
| `ctest --test-dir build` | 42+ 测试全绿 |
| `world_validator` | 0 error |
| Sim 回归 | seed100 50 局 F5 通过率在区间内 |

---

## 附录：数据流图

### 当前门交互流程（有缺陷）

```
WASD 输入
    │
    ▼
Player::handle_input() → 移动向量
    │
    ▼
PlayerController::tick()
    │
    ├─ position.x += move.x * dt
    ├─ is_rect_walkable()? ──NO──→ try_open_door_toward()
    │                                   │
    │                            是 CLOSED 门?
    │                             ├─ YES → 开门 + 重试移动（同帧穿过）
    │                             └─ NO → 回退（被弹回）
    │
    └─ position.y += move.y * dt (同上)
```

### 目标门交互流程

```
WASD 输入
    │
    ▼
Player::handle_input() → 移动向量
    │
    ▼
PlayerController::tick()
    │
    ├─ position.x += move.x * dt
    ├─ is_rect_walkable()? ──NO──→ 回退（被弹回，但门格子修复后不再误判）
    │
    └─ position.y += move.y * dt (同上)

E 键输入
    │
    ▼
PlayerController::handle_input()
    │
    ├─ NPC/物品/楼梯检查（现有逻辑）
    │
    └─ 4 方向 CLOSED 门检查
         ├─ 找到 → set_door_state(OPEN) + FOV 重算 + 位移到门格子
         └─ 没找到 → 走现有交互
```

### 当前 FOV 流程（跨房间穿透）

```
update_fov(cx, cy, radius)
    │
    ├─ 射线穿过 A 房间
    ├─ 射线穿过 OPEN 门（blocks_sight = false）
    ├─ 射线穿过走廊
    ├─ 射线穿过 OPEN 门
    ├─ 射线穿过整个 B 房间 ← 问题！
    └─ 直到 radius 用完或撞墙
```

### 目标 FOV 流程（房间截断）

```
update_fov(cx, cy, radius)
    │
    ├─ 射线穿过 A 房间（room_id = 0）
    ├─ 射线穿过 OPEN 门（记录 last_door）
    ├─ 射线到达走廊（room_id = -1，不截断）
    ├─ 射线穿过另一个 OPEN 门（记录 last_door）
    ├─ 射线到达 B 房间 1 格（room_id = 1）
    │    └─ player_room (0) != target_room (1) → 截断！
    └─ 结果：B 房间只看到门口 1 格
```
