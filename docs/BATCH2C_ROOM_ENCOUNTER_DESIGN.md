# Batch 2C: Room Encounter — Design Document

> **日期**: 2026-08-28
> **状态**: 设计完成 — **待用户审核，未编码**
> **前置**: Batch 1 (A1 seal + DoorState) ✅ · Batch 2B (R1 接触开门 + S1 Sim 语义) ✅
> **流程**: 审计 → **本设计** → 用户审核 → 编码
> **上游决策**: `ROOM_ENCOUNTER_DOOR_FOV_INTEGRATION_AUDIT.md` §8 (状态机) / §11 (职责边界)

---

## 1. 设计目标

在已确立的 Room→Door→Corridor 拓扑上实现以撒式房间战斗：

```
玩家进入有活怪的房间 → 封门 → 清房 → 开门
```

核心原则（源自审计 §8.1）：**Encounter 在实时战斗中已经是常态**（怪物直接布点房间），本批的"封门"是**空间封禁**而非战斗开始演出——玩家在封门前可能已被怪攻击（追出门口），关门提供竞技场边界与不可逃跑性。

## 2. 范围与不做

### 本批实现
- `RoomManager`（新，~200 行）：房间状态机 + 房间↔门组映射 + 触发/清房判定
- 门组操作：`GameMap::close_room_doors(room) / open_room_doors(room)`（原子性）
- 新事件：`ROOM_LOCKED` / `ROOM_CLEAR`
- 玩家 tile 跨房间检测（挂在 game_scene tick）
- Boss 房 / 特殊房 / 楼梯房 的边界规则（E4/E5/E7）
- sim S1 已就绪（Batch 2B），Room Encounter 直接复用

### 明确不做（Batch 3 / 后续）
- LOCKED / SEALED 门
- 封门演出动画/音效（仅一次性 room_msg 提示）
- 清房掉落奖励钩子（本期只发 ROOM_CLEAR 事件）
- Minimap 特殊房标记（与 Batch 3 捆绑）

## 3. 数据结构

### 3.1 RoomManager（新文件 `src/game/world/room_manager.h/.cpp`）

```cpp
enum class RoomEncounterState {
    IDLE,      // 无怪 / 已清房 / Boss 房非战斗阶段
    ARMED,     // 玩家已进入且房内存在活怪 (待锁)
    LOCKED,    // 门组已 CLOSED (战斗进行中)
    CLEARED    // 房内怪全灭, 门已 OPEN (本层内永久)
};

struct RoomEntry {
    RoomEncounterState state = RoomEncounterState::IDLE;
    int rx = 0, ry = 0, rw = 0, rh = 0;           // 房间矩形
    std::vector<std::pair<int,int>> door_tiles;    // 房间的门组 (含 A1 新增门)
    bool is_boss_room = false;
    bool is_special_room = false;
};

class RoomManager {
public:
    void build(GameMap* map, const std::vector<std::tuple<int,int,int,int>>& rooms,
               const std::vector<std::pair<int,int>>& special_room_centers);
    void tick(GameScene* gs, GameMap* map, Player* player,
              const std::vector<std::unique_ptr<Monster>>& monsters);
    bool is_room_locked(int room_idx) const;
    int room_at(int tx, int ty) const;   // 玩家所在房间索引 (-1=走廊)
private:
    std::vector<RoomEntry> _rooms;
    int _monster_in_room(const RoomEntry& r, GameMap* map,
                         const std::vector<std::unique_ptr<Monster>>& monsters) const;
    void _try_lock(RoomEntry& r, GameMap* map);
    void _try_unlock(RoomEntry& r, GameMap* map, GameScene* gs);
};
```

### 3.2 GameMap 门组操作（新增，配合 Batch 2B 的 door_state）

```cpp
// 原子关闭/开启房间所有门 (E3: 全部门组同闭同开)
bool GameMap::close_room_doors(const std::vector<std::pair<int,int>>& door_tiles);
bool GameMap::open_room_doors(const std::vector<std::pair<int,int>>& door_tiles);
// close: 逐扇 set_door_state(CLOSED); open: 逐扇 set_door_state(OPEN)
```

## 4. 状态机（审计 §8.2 落地）

```
        玩家中心 tile 进入房间内部(rect) 且房内存在活怪
IDLE ────────────────────────────────────────────────► ARMED(待锁)
   ▲                                                     │
   │ 清房(房内怪全灭)且门组无实体重叠                      │ 全部门组可安全关闭 (E1/E2/E3)
   │                                                     ▼
CLEARED ◄──────── 清房判定 ──────────────────────── LOCKED(封禁中)
```

### 4.1 触发条件（IDLE→ARMED）
- 玩家中心 tile 进入房间矩形内部（`room_at` 返回 >=0）
- 房内存在 `combat.is_alive` 的怪物
- 玩家不在门 tile 上（防止半个身子在门里触发）

### 4.2 封门条件（ARMED→LOCKED）
- **E1** 门组所有 tile 上无实体 rect 重叠（玩家 + 房内怪）
- **E2** 本房怪物全部在房间内部（leash 锚点在房内，正常情况下成立；若怪被击退/传送出门外，暂缓落锁直至其返回或被击杀）
- **E3** 全部门组原子关闭（多门房同时 CLOSED）

### 4.3 清房（LOCKED→CLEARED）
- 房内 `is_alive` 计数归零 → 开门（OPEN）+ 发 `ROOM_CLEAR` 事件
- 事件 payload: room_idx, floor

### 4.4 楼层边界
- `enter_floor` 时 `RoomManager::build` 重建（每层新房间）
- 死亡/重开：状态随楼层重建自然归零（存档不存房间状态——审计 §10 结论）

## 5. 边界规则清单（E1~E8 落地）

| # | 边界 | 规则 | 状态 |
|---|------|------|------|
| E1 | 关门瞬间实体压门 | ARMED 下每 tick 检测门组 rect 重叠，无重叠才落锁（<1s 窗口） | 实现 |
| E2 | 房怪在门外 | 落锁条件含“房内怪全在房内”；leash 自然配合 | 实现 |
| E3 | 多门房原子性 | close_room_doors 全组操作 | 实现 |
| E4 | Boss 房 (F5/10/15) | 不启用 Room Encounter（有 BOSS_INTRO/CINEMATIC 独立状态流） | 跳过 |
| E5 | 特殊房 (祭坛/商店等) | Room Encounter 正常启用（E 键交互不受影响） | 实现 |
| E6 | 玩家死亡/The World | 死亡走现有流程；时停冻结世界，门逻辑自然停摆 | 默认 |
| E7 | 楼梯房 (rooms.back) | 楼梯激活 = 清层；楼梯房若住怪照常 Encounter | 实现 |
| E8 | 击退/传送推出 LOCKED | CLOSED 门 = 碰撞墙（Batch 1 已保证），推不出去 | 默认 |

## 6. 职责边界（审计 §11 落地）

| 组件 | 拥有 | 不拥有 |
|------|------|--------|
| RoomManager | 房间状态机、房间门映射、触发/清房判定 | tile 碰撞/视线、伤害计算 |
| GameMap | 门组开闭 API、door_state | 房间语义 |
| GameSceneCombat | 伤害/击杀（现状不动） | 门逻辑；RoomManager 每帧查 is_alive |
| EventBus | ROOM_LOCKED/ROOM_CLEAR（+2 枚举） | — |
| PlayerController | R1 接触开门（Batch 2B） | 房间归属 |
| SimAI | S1（Batch 2B） | 不感知房间状态机 |

### 数据流（每帧）
```
GameScene::tick
  ├─ PlayerController::tick ──(R1 接触开门)
  ├─ RoomManager::tick(player_tile, monsters)
  │    ├─ room_at() 检测进入 → ARMED
  │    ├─ E1/E2/E3 通过 → close_room_doors + ROOM_LOCKED
  │    └─ LOCKED 中房内怪清零 → open_room_doors + ROOM_CLEAR
  └─ (FOV/爬楼 现状不变)
```

## 7. 影响面（文件级）

| 文件 | 改动 |
|------|------|
| 新增 src/game/world/room_manager.h/.cpp | RoomManager 实现 (~200 行) |
| src/game/world/game_map.h/.cpp | +close_room_doors/open_room_doors |
| src/game/scenes/game_scene.h/.cpp | 持有 RoomManager; enter_floor 调 build; tick 调 RoomManager::tick |
| src/game/core/event_types.h | +ROOM_LOCKED, ROOM_CLEAR |
| src/game/scene/game_scene_combat.cpp | 零改动 (on_monster_killed 不动) |
| tests | 新增 room_encounter_test.cpp |

## 8. 测试计划

| # | 测试 | 断言 |
|---|------|------|
| T1 | 触发 | 玩家进房 + 房内有怪 → ARMED; 无怪 → IDLE |
| T2 | 封门 | ARMED 且 E1/E2/E3 满足 → LOCKED + 全门 CLOSED |
| T3 | 清房 | 房内怪全灭 → CLEARED + 门 OPEN |
| T4 | 原子性 | 多门房: 关闭/开启全组一致 |
| T5 | 边界 | 玩家在门 tile 不触发; 房怪在门外不落锁; Boss 房跳过 |
| T6 | 集成 | RoomManager::build 对 27 seeds 门映射完整 |

## 9. 验收标准

1. room_encounter_test T1~T6 全绿
2. 41 既有测试全绿（seal / door_interact / fov 不回归）
3. Sim 回归：Batch 2C 引入真实关门 → Sim 会经历 LOCKED→CLEARED（S1 保证能开门）→ 500 局胜率可能变化，记录为新基线
4. 构建 0 警告；确定性保持
5. 实机：进有怪房→门关→清房→门开；Boss 房/特殊房行为正确

## 10. 待用户审核点

| # | 决策 | 推荐 |
|---|------|------|
| Q1 | Boss 房是否启用 Room Encounter | 跳过（现有 BOSS_INTRO 流程已封竞技场） |
| Q2 | 清房是否发掉落 | 本期只发 ROOM_CLEAR 事件，掉落钩子 Batch 3 接 |
| Q3 | 封门是否需要演出 | 仅一次性 room_msg，无动画 |
| Q4 | Sim 胜率变化接受度 | 接受为新基线（延续既有裁决） |

---

*本设计零代码改动。审核通过后按编码 → 测试 → 验收提交。*
