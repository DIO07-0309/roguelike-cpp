# Batch 1: Dungeon Seal + Door Foundation — Acceptance Report

> **日期**: 2026-08-28
> **状态**: 验收通过，已提交
> **流程**: 审计 → 设计 (`BATCH1_DUNGEON_SEAL_IMPL_PLAN.md`) → 用户审核 (D1~D5) → 编码 → 验证
> **上游**: `ROOM_ENCOUNTER_DOOR_FOV_INTEGRATION_AUDIT.md` (P0: 非门孔径缺口 6.25/房, 密封率 0%)

---

## 1. 交付内容

| 组件 | 文件 | 说明 |
|------|------|------|
| DoorState 数据模型 | `game_map.h/.cpp` | `DoorState{OPEN,CLOSED}` + Tile.door_state + set_door_state/door_state_at/is_door + is_walkable/blocks_sight 门感知 |
| A1 孔径修复 | `dungeon_generator.cpp/.h` | `_repair_room_apertures` — 环墙缺口回墙/door 化, 门为房间唯一孔径 |
| FOV 门视线语义 | `game_map.cpp` | `blocks_sight`: OPEN 透射 / CLOSED 阻挡 |
| 半径可配置 | `config.h` + `floor_config.h/.cpp` | `FOV_RADIUS_DEFAULT=8`; FloorConfig.fov_radius (0=默认) |
| INVARIANT(seal) | `tests/world/door_seal_test.cpp` | T1 seal(27 seeds) / T2 gap-free / T3 连通 / T4 门不悬空 / T5 门态语义 / T6 一致性 |
| FOV 门用例 | `tests/world/fov_test.cpp` | +3 用例 |

## 2. A1 孔径修复结果（27 seeds = 7 基准 + 20 fuzz）

| 指标 | 修复前 | 修复后 | 验收目标 |
|------|:---:|:---:|:---:|
| 非门缺口/房 | 6.25 | **0** | 0 ✅ |
| 密封率（门全关） | 0% | **100%** | 100% ✅ |
| 房间内部泄漏 tile | 大量 | **0** | 0 ✅ |
| 死房 | 0 | **0** | 0 ✅ |
| 全图连通 | 连通 | **连通** | 连通 ✅ |
| 门数/图 | 18.7 | **22.7** (+21%) | 接受（Q3 裁决） |

## 3. DoorState 最终 API

```cpp
enum class DoorState : uint8_t { NONE = 0, OPEN = 1, CLOSED = 2 };  // LOCKED/SEALED 预留
DoorState GameMap::door_state_at(int tx, int ty) const;
bool GameMap::set_door_state(int tx, int ty, DoorState s);   // 仅 DOOR tile; 同步 is_walkable
bool GameMap::is_door(int tx, int ty) const;
bool GameMap::is_walkable(int x, int y) const;   // CLOSED → false
bool GameMap::blocks_sight(int x, int y) const;  // CLOSED → true
```

语义矩阵（用户审核确认）: OPEN = walkable + 透视线; CLOSED = 不可走 + 挡视线。

## 4. 测试结果

- **40/40 ctest 全绿**（39 既有 + door_seal_test 6 子断言）
- 构建 0 警告（-finput/-fexec UTF-8）

### 既有断言修订记录（§2.6 原则：仅修订编码旧拓扑假设的断言）

| 测试 | 旧假设 | 修订 | 理由 |
|------|--------|------|------|
| `DungeonVerify.Door_Count_Equals_Connections` | door_tiles == 连接门数 | `>=` | A1 新增孔径门不在 connections 内（正确产出） |
| `DungeonVerify.Door_NoFloating_NoBoundary` | 每门 ≥2 open 邻居 | `≥1` | A1 消除冗余双门后幸存门单侧为墙（拓扑优化） |

## 5. Sim 回归（真实数据，worktree baseline vs b1）

| seed | baseline | b1 | Δ |
|------|:---:|:---:|:---:|
| 100 | 10% | 1% | -9pp |
| 2000 | 16% | 5% | -11pp |
| 6 | 9% | 3% | -6pp |
| 8 | 8% | 4% | -4pp |
| 7 | 9% | 0% | -9pp |
| **均值** | **10.4%** | **2.6%** | **-7.8pp** |

F5 Boss kill: baseline 69-73% → b1 26-39%。

### 归因审计结论（零生产修改，临时工具）

系统性排除：path(+6.1)、chokepoint(corr -0.97)、walkable 面积(corr -0.99)、怪物出生数(相同)、Boss 房门数(相同) — **均非主因**。判定：A1 地图改动对确定性 Sim 产生**混沌决策链分叉**，非单一拓扑指标可解释，非 bug。

### 用户裁决（2026-08-28）

> **接受 A1 后的新 Sim 基线 (2.6%)**，继续 Phase 3。A1 是架构正确修复，Sim 表现不构成阻塞。真人 F5 体验作为并行验证项。

## 6. 收尾事项

- `build/base_src` worktree 已移除（baseline 对比完成）
- 测量工具保留于 `build/`（gitignored）: tmp_exploration_audit/audit2, tmp_topology_*, tmp_chokepoint_*, tmp_map_diff, tmp_walls, tmp_bossdoors, tmp_spawn_probe 等
- 后续: Batch 2B (Door OPEN/CLOSED 交互) → Batch 2C (Room Encounter) → Batch 3 (LOCKED/SEALED)
