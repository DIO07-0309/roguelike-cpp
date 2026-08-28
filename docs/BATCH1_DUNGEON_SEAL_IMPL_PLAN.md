# Batch 1: Dungeon Seal + Door Foundation — Detailed Implementation Plan

> **日期**: 2026-08-28
> **状态**: 设计完成 — **待用户审核，未编码**
> **流程**: 审计 (`ROOM_ENCOUNTER_DOOR_FOV_INTEGRATION_AUDIT.md`) → 用户审核 (D1~D5 已表态) → **本计划** → 用户审核 → 编码
> **上游决策落地**: D1=A1 · D2=门默认 OPEN 独立验证 · D3=S1(零特判) · D4=半径可配默认 8 · D5=Batch 1→2→3

---

## 0. 范围与不做

### 0.1 Batch 1 交付（用户定义）

1. **A1 孔径修复** — 消除全部 Room→Corridor 非法缺口
2. **INVARIANT(seal) 永久回归** — "门是房间唯一孔径"成为永久测试
3. **DoorState 数据模型** — OPEN/CLOSED 双态 + 语义（OPEN: walkable=true/blocks_sight=false；CLOSED: 反之）
4. **FOV 支持 Door 视线语义** — `blocks_sight` 门感知
5. **门默认 OPEN** — D2：Batch 1 无任何关闭路径，用于独立验证孔径修复与 FOV 效果
6. **audit2 重新量化验收** — 密封率 100%、Non-Door Gap=0、门不悬空、全图连通、无死房

### 0.2 明确不做（顺延 Batch 2/3）

- ❌ 门默认 SHUT / R1 接触开门（D2 调整：Batch 2）
- ❌ RoomManager / 状态机 / ROOM_* 事件（Batch 2）
- ❌ LOCKED/SEALED 状态（Batch 3）
- ❌ FOV 半径数值调整（D4：保持 8，先修空间再测量）
- ❌ Minimap SHUT 色变体（门恒 OPEN，现色即正确；Batch 2/3）
- ❌ SimAI 修改（见 §2.6：Batch 1 无 SHUT 态出现于 gameplay，BFS 语义天然不变）

---

## 1. A1 修复算法 — 原型已验证（audit2 演练数据）

算法已在 `build/tmp_exploration_audit2.cpp` 于 7 seeds 真实 GameMap 上完整演练（分类→试墙→连通裁决→door 化→验证），**以下是实测结果，非推断**：

| seed | 缺口(走廊型/房间直连型) | 回墙 | 转门 | 门数 | 缺口残留 | 泄漏内部 tile | 死房 |
|------|------|:---:|:---:|------|:---:|:---:|:---:|
| 1 | 80 / 0 | 78 | 2 | 20→22 | 0 | 0 | 0 |
| 2 | 75 / 0 | 69 | 6 | 18→24 | 0 | 0 | 0 |
| 3 | 88 / 0 | 80 | 8 | 20→28 | 0 | 0 | 0 |
| 42 | 57 / 0 | 55 | 2 | 14→16 | 0 | 0 | 0 |
| 100 | 57 / 0 | 57 | 0 | 17→17 | 0 | 0 | 0 |
| 999 | 68 / 0 | 63 | 5 | 22→27 | 0 | 0 | 0 |
| 20240801 | 70 / 0 | 65 | 5 | 20→25 | 0 | 0 | 0 |
| **Σ** | **495 / 0** | **467 (94.3%)** | **28 (5.7%)** | **131→159** | **0** | **0** | **0** |

**原型结论（写进设计）：**
1. 缺口全部为走廊型（`rr=0`），不存在房间-房间直连 → 算法无需处理跨房分支（但保留分类判断以防 future seeds 出现）。
2. 94.3% 缺口可安全回墙（多数是 `_carve_diamond` 留下的死凹槽）；5.7% 是活的走廊穿行点，door 化处理。
3. 门数 18.7→22.7/图（+21%），无死房、全连通保持。
4. **测试规范要点（原型中踩到的伪影）**：seal 检查的 BFS 必须从**走廊 tile** 出发 — 从出生房中心出发会把出生房自身内部计为"可达"（伪影）。INVARIANT 测试按此实现。


---

## 2. 修改明细（按文件）

### 2.1 `src/game/world/game_map.h` / `game_map.cpp` — DoorState 数据模型

**game_map.h：**

```cpp
enum class DoorState : uint8_t { NONE = 0, OPEN = 1, CLOSED = 2 };  // LOCKED/SEALED Batch 3 预留

struct Tile {
    TileType type = TileType::WALL;
    bool is_walkable = false;
    bool is_visible = false;
    bool is_explored = false;
    DoorState door_state = DoorState::NONE;   // 仅 DOOR tile 非 NONE (+1B/tile ≈ 1.2KB)

    static Tile floor()  { return {TileType::FLOOR,  true,  false, false, DoorState::NONE}; }
    static Tile wall()   { return {TileType::WALL,   false, false, false, DoorState::NONE}; }
    static Tile stairs() { return {TileType::STAIRS_DOWN, true, false, false, DoorState::NONE}; }
    static Tile lava()   { return {TileType::LAVA,   true,  false, false, DoorState::NONE}; }
    static Tile door()   { return {TileType::DOOR,   true,  false, false, DoorState::OPEN}; } // D2: 默认 OPEN
};
```

GameMap 新增 API：

```cpp
DoorState door_state_at(int tx, int ty) const;          // 非 DOOR tile 返回 NONE
bool set_door_state(int tx, int ty, DoorState s);        // 非 DOOR tile 返回 false
bool is_door(int tx, int ty) const;                      // tile_at == DOOR 便捷查询
```

**game_map.cpp：**

```cpp
bool GameMap::set_door_state(int tx, int ty, DoorState s) {
    if (!_in_bounds(tx, ty)) return false;
    auto& t = _tiles[ty][tx];
    if (t.type != TileType::DOOR) return false;
    if (t.door_state == s) return true;
    t.door_state = s;
    t.is_walkable = (s == DoorState::OPEN);   // 语义落点: OPEN=可走, CLOSED=不可走
    return true;
}

bool GameMap::blocks_sight(int x, int y) const {
    if (!_in_bounds(x, y)) return true;
    const auto& t = _tiles[y][x];
    if (t.type == TileType::WALL) return true;
    if (t.type == TileType::DOOR) return t.door_state == DoorState::CLOSED;  // ← 唯一视线改动
    return false;
}
```

设计要点：
- **`is_walkable` 物化更新**（热路径零改动）：全部实体碰撞走 `is_walkable` 物化字段，门态切换仅在 `set_door_state` 单点改写 — 唯一写入口，无散落同步。
- **`blocks_sight` 即时求值**（调用点少、非热路径每 tile 每帧必查）——两种策略按各自路径特性选择。
- `update_fov` 算法零改动（只依赖 `blocks_sight`）。
- `load_from_template`：`'D'` → `Tile::door()`（已存在则仅确认 door_state=OPEN）。
- 不变量（测试断言）：`DOOR tile ⟹ is_walkable == (door_state == OPEN)`。

### 2.2 `src/game/world/dungeon_generator.cpp` — A1 孔径修复（照原型移植）

**接入点**（generate() 内，模板域操作，先于 load_from_template）：

```cpp
auto tmpl = _build_template();
_repair_room_apertures(tmpl);        // ← Batch 1 新增
gm->load_from_template(tmpl);
```

**算法**（= 已验证原型逐行移植到 char grid 域；**零 rng 消耗，种子确定性保持**）：

```
_repair_room_apertures(grid):
  # 1. 收集缺口: 逐房扫描环墙 cell (上/下/左/右), walkable 且非 'D' → gap
  #    分类: cell 位于其他房间 rect 内 → room_room 型 (原型 rr=0, 保留分支防御)
  # 2. 逐 gap 决策 (固定顺序 = 房间索引序, 确定性):
  #      tentative grid[y][x] = '#'
  #      BFS(spawn=rooms[0] 中心, walkable = '.' 或 'D')
  #      if visited == 全图 walkable 总数: keep '#'      # 回墙 94.3%
  #      else: grid[y][x] = 'D'                          # 走廊穿行点 → door 化 5.7%
  # 3. 内建自检 (generate 内 debug 断言 + 测试外置永久回归):
  #      复扫环墙无 '.' | 全门视为墙 BFS 从走廊 tile → 无房间内部可达 | 门全开 → 全房可达
```

实现细节：
- BFS/连通检查为 dungeon_generator.cpp 文件内 static 函数（~40 行），char grid 域。
- **共享环 cell**（两房间隔 2 列时 A 右环 = B 左环）按房间索引序只处理一次，第二次扫描已见 '#'/'D' 自然跳过 — 确定性无双重处理。
- 门组（group）概念**不在 Batch 1 落地**（RoomManager 才消费门组，Batch 2 建）；本批 door 化的多 tile 孔径各自为独立 DOOR tile（均 OPEN），Batch 2 建映射时按相邻 DOOR 聚类。

### 2.3 FOV Radius 可配置（D4：默认仍 8）

| 文件 | 改动 |
|------|------|
| `config.h` | `inline constexpr int FOV_RADIUS_DEFAULT = 8;` |
| `floor_manager.h` (FloorConfig) | `int fov_radius = 0;   // 0 = 用 FOV_RADIUS_DEFAULT；Batch 1 全部 0（不改层数值）` |
| `game_scene.h` | 删除 `static constexpr int FOV_RADIUS = 8`（:289，全仓唯一消费点 :957）；新增 `int _fov_radius = FOV_RADIUS_DEFAULT;` |
| `game_scene.cpp` | `enter_floor`：`_fov_radius = fcfg->fov_radius > 0 ? fcfg->fov_radius : FOV_RADIUS_DEFAULT;`；:957 改用 `_fov_radius` |

影响面：fov_test 传显式半径不受影响；Minimap 不感知半径；`update_fov` 成本微秒级。

### 2.4 新增测试 `tests/world/door_seal_test.cpp`

| # | 断言组 | 内容 |
|---|--------|------|
| T1 | **INVARIANT(seal)** | seeds {1,2,3,42,100,999,20240801} + fuzz {1000..1019}（27 张图）：收集全部 DOOR → 找走廊起点（walkable 非 DOOR 非房间内部）→ BFS（DOOR 视为墙）→ **断言任何房间内部 tile 均不可达** |
| T2 | gap-free | 每房环墙 cell ∈ {WALL, DOOR}（Non-Door Gap = 0） |
| T3 | 全连通无死房 | 门全开 BFS → 全部 walkable 可达（等价覆盖：无孤岛 + 每房可达） |
| T4 | 门不悬空 | 每扇门：界内；≥2 open 邻居；至少一侧邻接房间内部 |
| T5 | DoorState 语义 | SHUT → `is_walkable==false ∧ blocks_sight==true`；OPEN → 反之；非门 tile `set_door_state` 返回 false；一致性 `is_walkable == (door_state==OPEN)` |

注册：`tests/CMakeLists.txt` + `add_roguelike_test(door_seal_test world/door_seal_test.cpp)`。

### 2.5 `tests/world/fov_test.cpp` 增 3 用例

```
DoorOpen_TransparentAndWalkable   # 现状行为锁定
DoorClosed_BlocksSight            # SHUT → update_fov 后门后 tile 不可见, 门 tile 自身可见
DoorClosed_NotWalkable            # SHUT → is_walkable false
```

### 2.6 既有测试影响审查（预期与处理原则）

| 测试 | 预期 | 处理 |
|------|------|------|
| dungeon_topology_test (12) | 大概率过（结构性质不变）；若存在"每连接恰 2 门"类精确计数断言会破 | 修订为 `≥2` 并在验收报告记录修订理由 |
| dungeon_verify_test (5) | 过（房间完整/连通/门合法 — 修复只改善） | 不动 |
| fov / minimap / save / 其余 22 | 不受影响 | 不动 |

原则：**只有当断言编码了"缺口存在"这一旧假设时才允许修订**，修订必须逐条记录进验收报告。

### 2.7 零改动声明（本批验证点）

| 组件 | 原因 |
|------|------|
| `sim_ai.cpp` | Batch 1 gameplay 中不存在 SHUT 门（默认 OPEN 且无关闭路径）→ BFS 语义不变；S1 门语义随 Batch 2 的 R1 接触开门一起进 |
| `save_manager.*` | v3 不存门（审计 §10 证据链） |
| `minimap.cpp` | 门恒 OPEN，现色正确 |
| `ai.cpp` / `player_controller.cpp` | 无关门发生；碰撞字段物化无感知 |
| `resources/*.json` | 无新配置（fov_radius 在 FloorConfig 代码默认 0） |

---

## 3. 验收标准（Batch 1 完成定义）

| # | 验收项 | 标准 | 验证手段 |
|---|--------|------|---------|
| A1 | INVARIANT(seal) | 27 seeds（7 基准 + 20 fuzz）：全门视为墙后，走廊 BFS **0 个房间内部 tile 可达** | door_seal_test T1 |
| A2 | Non-Door Gap | **= 0**（每房环墙 ∈ {WALL, DOOR}） | T2 + audit2 `gaps_left=0 / rooms_with_gap=0%` |
| A3 | 门不悬空 | 每门界内、≥2 open 邻居、邻接房间内部 | T4 + dungeon_verify 既有断言 |
| A4 | 全图连通无死房 | 门全开 BFS：全部 walkable 可达、每房可达 | T3 + audit2 `dead=0` |
| A5 | 既有回归 | **39/39 ctest 全绿**（修订逐条记录） | ctest |
| A6 | 量化复测 | audit1/audit2 全量重跑，数字写入验收报告：①OPEN 门态泄露（预期因孔径收窄而下降，具体值记录为 Batch 2 before 基线）②SHUT 模拟泄露 **≈0%**（视线语义端到端验证） | build/ 工具（gitignored） |
| A7 | Sim 回归 | 500 局胜率 6~10% 带内；`reports/baseline_s*.json` 刷新（R6 布局变化）；确定性对拍一致 | `--sim` 批跑 |
| A8 | 实机 | 多 seed smoke 无崩溃；目测门密度/布局观感 | 人工 |

---

## 4. 实施顺序（每步带验证点）

| 步 | 内容 | 验证 |
|----|------|------|
| S1 | game_map DoorState 模型 + set_door_state + blocks_sight 门感知 | fov_test 新 3 用例 + T5 编译过（T5 正式落 door_seal_test） |
| S2 | config.h / FloorConfig / game_scene 半径可配 | 全量构建 0 警告 + 冒烟（FOV 行为与改前逐像素一致——默认 8） |
| S3 | dungeon_generator `_repair_room_apertures`（原型移植） | 39 测试跑 → 暴露的旧假设断言按 §2.6 原则处理并记录 |
| S4 | door_seal_test 编写 + 注册 | T1~T5 全绿（含 fuzz seeds） |
| S5 | audit1 + audit2 复测 | A6 数字落盘 |
| S6 | 500 局 sim 回归 + baseline 刷新 + 确定性对拍 | A7 |
| S7 | 实机 smoke + 桌面打包版同步 | A8 |
| S8 | 验收报告 `docs/BATCH1_DUNGEON_SEAL_ACCEPTANCE.md` + CHANGELOG + 提交 | — |

提交切分（3 个 commit）：①`feat: DoorState model + FOV door sight semantics` ②`feat: A1 aperture repair + INVARIANT(seal)` ③`docs: Batch 1 acceptance report`。

---

## 5. 风险与回滚

| # | 风险 | 等级 | 缓解 |
|---|------|:---:|------|
| R1 | 回墙破坏连通 | 已内建降级 | 试墙 BFS 失败即 door 化（原型实证 467/495 回墙成功，28 自动降级 door 化） |
| R2 | 旧断言暴露 | 中 | §2.6 处理原则 + 记录；不允许静默改测试 |
| R3 | 布局变化 → sim 基线漂移 | 确定 | A7 重采基线；对拍同版本自洽 |
| R4 | seal 测试起点伪影 | 已排雷 | 测试规范固化：BFS 必须从走廊 tile 出发（§1 结论 4） |
| R5 | 门数 +21% 视觉密度 | 低 | 观察项；实机确认；密度过高则 A1 加"孔径合并"微调（预留） |
| 回滚 | Batch 1 改动集中在 generator/map 两文件 + 半径常量 | — | 单 commit revert 即可整体回退；无存档/平衡耦合 |

---

## 6. 交付物清单

- 代码：`game_map.h/.cpp`、`dungeon_generator.h/.cpp`（声明+实现）、`config.h`、`floor_manager.h`、`game_scene.h/.cpp`
- 测试：`tests/world/door_seal_test.cpp`（新）、`fov_test.cpp`（+3）、`tests/CMakeLists.txt`（+1 行）、既有断言修订（如有，记录）
- 文档：`docs/BATCH1_DUNGEON_SEAL_ACCEPTANCE.md`（含 audit1/audit2 复测数字 + 断言修订记录）、CHANGELOG 条目、README Feature 段落一句更新（孔径完整性）

## 7. 待确认点（审核本计划时表态，默认按推荐执行）

| # | 点 | 推荐 | 备选 |
|---|-----|------|------|
| Q1 | fuzz seed 数 | 20（合计 27 图，跑时 <1s） | 50 |
| Q2 | FloorConfig.fov_radius 字段 | 纳入 Batch 1（2 行，数据驱动通道打通，数值全 0 不生效） | 退 Batch 2 |
| Q3 | 门数增长 18.7→22.7/图 | 接受（孔径即门，语义正确） | 后续 A1 加孔径合并降低门数 |
| Q4 | `FOV_RADIUS` 旧名兼容 | 直接删除（全仓唯一消费点，无外部依赖） | 保留 deprecated 别名 |


