# Room Encounter + Door State + FOV Integration — Pre-Implementation Audit

> **日期**: 2026-08-28
> **状态**: 审计完成，含设计方案选项 — **待用户审核，未编码**
> **范围承诺**: 零代码改动。测量工具位于 `build/`（gitignored，已按用户指示保留作诊断工具）
> **上游输入**: `EXPLORATION_EXPERIENCE_AUDIT.md` (P1 隔门泄露 93.2% / P2 进房即 100% 揭示)
> **用户决策输入**: P1/P2 与 Room Encounter 统一设计避免返工；Phase 4 暂不编码

---

## 0. 结论速览 (Executive Summary)

1. **🔴 新发现 P0（本次审计最重要产出）**：**当前地牢"关门封不住房间"**。`_carve_diamond` 雕走廊时在房间环墙上留下大量非门缺口 — 平均 **6.25 个/房**、**100% 的房间存在缺口**（最多 13 个）。实测：关闭全部门 tile 后房间可达率仍为 **0% 密封**，隔门泄露只从 93.2% 降到 32.9%（缺口也是透视线）。**任何门状态机/房间战斗在孔径修复之前实施都必然返工** — Phase 4a「门孔径完整性」是全链路的第一前置。
2. **Save v3→v4 不需要**（焦点 8 的硬答案）：存档只发生在清层/退出时点（game_scene.cpp:1528 / game_scene_input.cpp:66），`SaveData` 不保存怪物状态，读档 = seed 重生楼层 → 门状态天然重置为初始态。**SaveManager 零改动**。
3. **FOV 半径不是探索深度的杠杆**（焦点 4/5 的数据答案）：房间 min_dim ≤6 的占 87%（37+32/79），半径 4~8 的中心全揭示率 64%→100%，**调半径救不了房间内纵深**；半径可配置化照做（成本≈0），但深度的真正杠杆是"关门+开门揭示节拍"与（可选的）房间尺寸上调。
4. **"每步 25% 随机战斗"已不存在**：全仓 grep 无 battle cards / 随机触发残留，战斗自实时化改造后就是"怪物直接布点房间 + leash 守卫"（floor_manager.cpp:43）。Room Encounter 的设计基线是**在实时战斗上加房间锁定**，不是替换随机遇敌。
5. **SimAI 可以零特判**：采用"接触开启"统一规则（玩家与 sim 的 BFS 都把 CLOSED 门视为可通行、接触即开），sim 确定性与 500 局回归可比性保住。

---

## 1. 方法与取证范围

- 量化工具 `build/tmp_exploration_audit2.cpp`：本地复刻 `GameMap::update_fov` 语义（360 射线 / 0.5 步进 / 先标记后 break），增加 `doors_block` 开关模拟 CLOSED 门；BFS 密封性检查；孔径普查。链接真实游戏 obj（同 audit1）。
- 代码取证：combat/save/event/sim/floor_manager/leash 全链路阅读（引用行号见各节）。
- 本审计回答用户指定的 10 个重点（§2~§11 逐一对应），并给出分期建议（§13）与待决策清单（§16）。

### 1.1 关键代码事实（本审计基准）

| 事实 | 位置 |
|------|------|
| 怪物出生：房间中心 ±2 offset，跳过 rooms[0]（玩家出生房） | floor_manager.cpp:43-72 |
| leash：出生锚点，巡逻 4.5 / 追击 8.0 tiles，Boss 豁免 | ai.h:97-102 |
| 楼层清空判定：`is_floor_cleared` = 全怪死亡 → `_activate_stairs` | floor_manager.cpp:74-78, game_scene.cpp:1505 |
| 存档时点：清层 (_activate_stairs) + Esc 退出；怪物/门不保存 | game_scene.cpp:1528, game_scene_input.cpp:66 |
| `blocks_sight` 仅 WALL | game_map.cpp:83-88 |
| `FOV_RADIUS = 8` 为 GameScene 类常量 | game_scene.h:289 |
| EventBus 事件（GameEventType）：无任何 ROOM_*/DOOR_* 事件 | event_types.h:8 |
| 世界事件（EventType，含 AMBUSH 四周刷怪）独立于 EventBus | event_system.h:14 |
| SimAI BFS 可走判定：`_tile_rect_walkable`（rect 级），毒池规避 | sim_ai.cpp:283-365 |
| 玩家移动碰撞：rect 级 `is_rect_walkable` | entity.h/game_map.h:53 |

---

## 2. 焦点 1：OPEN/CLOSED Door 对 is_walkable / blocks_sight 的完整语义

### 2.1 语义矩阵（设计提案）

| DoorState | is_walkable | blocks_sight | FOV 行为 | 碰撞行为 | Minimap |
|-----------|:---:|:---:|----------|----------|---------|
| OPEN（默认） | ✓ | ✗（现状保持） | 穿透（现状） | 可通过 | 棕色（现状） |
| CLOSED | **✗** | **✓** | 射线在门 tile 标记门可见后 break（门后全黑） | 四向阻挡；实体 rect 不得进入 | 深灰+锁纹色 |
| LOCKED（Batch 3） | ✗ | ✓ | 同 CLOSED | 同 CLOSED，需钥匙交互 | 灰蓝 |
| SEALED（Batch 3） | ✗ | ✓ | 同 CLOSED | 永久，需特殊条件 | 暗红 |

- 语义原则：**`is_walkable` 与 `blocks_sight` 同步切换**（CLOSED 门既是墙也是视线遮挡）——半开门（挡视线不挡人）会制造"看得见走不进"的违和，且无玩法收益。
- LOCKED/SEALED 复用 CLOSED 的全部行为，仅开启条件不同 → 状态机收窄为 {OPEN, SHUT} 二态 + {free, key, sealed} 三种开启策略，实现最小化。

### 2.2 数据结构选型

| 方案 | 内容 | 优劣 |
|------|------|------|
| **D1（推荐）** | `Tile` 增加 `uint8_t door_state`（0=非门, 1=OPEN, 2=SHUT…）；`GameMap` 增加 door 状态查询/设置 API；**门组**：同一孔径的相邻门 tile 共享 group_id | 热路径 O(1)（碰撞/FOV 每 tile 查询）；延续 Phase 1"Tile 扩展"决策（方案 1）；内存 +1B/tile ≈ 1.2KB |
| D2 | 独立 `DoorInstance` 表 + tile→door 映射 | Tile 保持 POD，但碰撞/FOV 热路径需二次查表 |

- **门组（group）必要性**：孔径可能宽于 1 tile（§4 修复后仍可能 2~3 tile 宽的门洞）→ 关门 = 整组同闭。group_id 编码进 door_state 高位或由 RoomManager 维护 tile→group 映射（推荐后者，Tile 保持简单）。
- 房间 ↔ 门组映射：由 `DungeonGenerator::get_connections()` + 房间矩形在进层时构建一次，存入 **RoomManager**（新，见 §10）。

---

## 3. 焦点 2：CLOSED Door 是否阻挡 FOV

**结论：阻挡（语义如上），但在当前生成器下收益有限 — 必须先做 §4 孔径修复。**

实测（7 seeds，FOV r=8，本地 FOV 复刻验证过与 `update_fov` 同参数）：

| 场景 | 隔门泄露 (门外1格见后方房) | 走廊平均可见房间数 | 走廊可见房间内部 tile |
|------|:---:|:---:|:---:|
| 全部 OPEN（现状） | 93.2% | 2.27 | 70.5 |
| **仅关 door tile（孔径缺口漏光）** | 32.9% | 2.14 | 58.5 |
| 孔径完整修复后（推算，缺口全封） | **≈0%** | **≈0** | **≈0** |

- 射线行为：CLOSED 门 tile 自身被标记 visible+explored（玩家知道"这里有扇门"），门后房间不点亮 — 这正是期望的探索语言："看见门，不知道门后是什么"。
- 开门瞬间：门组切 OPEN → 触发一次 `update_fov` 重算（挂在门状态变更处，非每帧）→ 房间揭示节拍（§5）。

---

## 4. 🔴 P0 前置：Door Aperture Integrity（门孔径完整性）

### 4.1 问题量化（7 seeds，79 个房间）

| 指标 | 值 |
|------|-----|
| 每房环墙上 walkable **非门** 缺口 | **平均 6.25 个**（最多 13） |
| 存在缺口的房间 | **100%** |
| 关闭全部 door tile 后仍可达的房间 | **100%（密封率 0%）** |
| door tile 本身作为有效孔径 | 100% ✓ |

根因：`_carve_diamond`（dungeon_generator.cpp:335-343）以走廊路径为中心雕菱形（半径 1~2），走廊与房间平行走段把环墙整段雕穿 — 这些缺口没有 DOOR 语义，是"隐形门"。

### 4.2 修复选项

| 方案 | 内容 | 优劣 | 判定 |
|------|------|------|------|
| **A1（推荐）** | **生成后修复通道**：逐房扫描环墙缺口 — 缺口外侧 tile 可走（活孔径）→ 升级为 DOOR（并入最近门组）；外侧是墙（死凹槽）→ 回填 WALL。回填后跑全图 BFS 连通性验证（复用 dungeon_verify 逻辑） | 精确、保留现有 carve 逻辑；需要"回填不破坏连通"验证；每房门数会上升到 2~5 | **推荐** |
| A2 | 修改 `_carve_diamond` 本体：雕凿时禁止触碰任何房间环墙 tile（除非是指定门位） | 根治；但走廊可能因绕行被切断，需耦合连通性重试循环，风险与工作量最高 | 备选 |
| A3 | 把所有缺口都改成门（人人有门） | 最简单；但平均 8 门/房稀释门的语义，房间战斗关门动画/逻辑变滑稽 | 不推荐 |

### 4.3 新增永久不变量（进 tests/world，与 dungeon_verify 并列）

```
INVARIANT(seal): 对任意 seed，将所有 DOOR 视为墙后，从任一走廊 tile 出发 BFS，
                 任何房间内部 tile 均不可达；且每房 ≥1 扇门，开门后房间可达。
```
> 这是"门"这个概念在代码中第一次拥有数学定义——此前 Phase 2 的不变量只验证了门存在且合法，没验证"门是唯一孔径"。


---

## 5. 焦点 3：玩家在走廊 / 门口 / 房间内的视野设计

### 5.1 三场景目标视野（孔径修复 + 门态实装后）

| 场景 | 玩家所见 | 设计意图 | 量化预期 |
|------|----------|----------|----------|
| **走廊行进** | 走廊本体 + 两侧墙上"已知的门"（门 tile 亮、门后黑） | 悬念驱动：选择开哪扇门 | 可见房间数 2.27 → **≈0** |
| **门口（门 CLOSED）** | 门 tile 高亮 + 门缝方向暗示（可选） | 开门决策点（尤其怪声提示后） | 门后房间 0% 可见 |
| **开门瞬间** | 门组 OPEN → FOV 重算 → 房间**一次性揭示至半径** | 肉鸽揭示节拍（Rogue/以撒混合） | 揭示率 = 半径曲线值（§6） |
| **房间内战斗（Room Encounter 锁定）** | 门组 CLOSED 切回，房间成为舞台 | 竞技场感；无法逃跑 | 门后走廊 0% 泄露 |

### 5.2 交互规则（开门方式）

| 规则 | 内容 | 判定 |
|------|------|------|
| **R1 接触开启（推荐）** | 玩家移动意图进入 CLOSED 门 tile → 自动开启（无按键），开启即 FOV 重算 | 零学习成本；sim 零特判（§8）；保留"E 键交互"给特殊房 |
| R2 按键开启 | E 键对门交互 | 多一步操作，走廊节奏变顿；不推荐为默认 |
| R3 视线开启 | 门进入玩家 FOV 即自动开 | 退化为现状泄露，否决 |

- 接触开启的碰撞细节：开启发生在**移动判定之前**（`can_move_to` 命中 CLOSED 门 → 先置 OPEN 再重新判定），避免"顶门不动一帧"。
- 关门瞬间若实体 rect 与门 tile 重叠（§7 状态机处理）：**延迟关门**至无重叠（见 §7.3-E1）。

---

## 6. 焦点 4：FOV Radius 是否改为可配置参数

**结论：改为可配置（成本≈0，照做），但数据表明它不是深度杠杆（§7）。**

- 现状：`static constexpr int FOV_RADIUS = 8` 硬编码于 game_scene.h:289。
- 提案：
  1. 迁移到 `config.h`（全局默认值，与 TILE_SIZE 等同级）；
  2. `FloorConfig` 增加可选 `fov_radius`（0 = 用默认）→ 数据驱动的逐层视野（F6-10 深渊层可以更暗、F11-15 虚空更远等，为 biome 主题服务）；
  3. 影响面：`update_fov` 调用点仅 1 处（game_scene.cpp:957）+ tests 显式传参不受影响 + Minimap 不感知半径 → **改动 ≈ 3 行 + FloorConfig 字段**。
- 性能：`update_fov` 成本 O(360 × r/0.5)，r=4~16 均为微秒级，无顾虑。

## 7. 焦点 5：房间尺寸与 FOV Radius 的合理比例

### 7.1 测量数据（半径曲线，7 seeds × 79 房）

| FOV 半径 | 站房中心 100% 全揭示 | 平均可见比例 |
|:---:|:---:|:---:|
| 4 | 64.1% | 96.0% |
| 5 | 87.6% | 99.3% |
| 6 | 96.0% | 99.9% |
| 7 | 100% | 100% |
| 8（现状） | 100% | 100% |

房间 min_dim 分布：≤5 = 37、6 = 32、7 = 4、8 = 4、9 = 2、10+ = 0 → **87% 的房间最小边 ≤6**。

### 7.2 数据结论

1. 想靠半径制造"房内暗角"需要 r ≤ min_dim/2 ≈ **3**——实时动作游戏里视野直径 6 tiles 属于不可用的幽闭参数，**否决"纯调半径"路线**。
2. 合理配比结论：**保持 r=6~8（推荐 8，或 per-floor 6~8），房间内纵深改由"门控揭示序列"提供**（进房 → 部分揭示 → 战斗/走位中逐步展开 → 清房开门）。深度来自信息节拍，不是视距裁剪。
3. （可选、后期批次）`DUNGEON_MIN_ROOM 5→7` 或 `min_part 8→10` 上调房间尺寸：能给 r=6~8 留出 15~25% 暗角，但**改变怪物密度/接战距离/sim 平衡**，属于独立平衡事件，建议与遗物系统 M1B 之后再排，不阻塞本链路。

---

## 8. 焦点 6：Room Enter → Door Close → Encounter → Clear → Door Open 状态机

### 8.1 前提修正

"Encounter" 在本游戏中已是**实时战斗**（怪物出生即布在房间内，无随机遇敌转场）。因此状态机锁定的是"有活怪的房间"，而非"触发一场战斗" — 比以撒式转场更轻：**关门本身不是战斗开始演出，而是空间封禁**。

### 8.2 房间状态（RoomManager 持有）

```
        玩家中心 tile 进入房间内部(rect) 且房内存在活怪
IDLE ────────────────────────────────────────────────► ARMED(待锁)
   ▲                                                     │
   │ 清房(房内怪全灭)且无实体重叠门组                      │ 全部门组可安全关闭
   │                                                     ▼
CLEARED ◄──────── 清房判定 ──────────────────────── LOCKED(封禁中)
                                          │
                              LOCKED 期间: 怪物追击/玩家战斗照常
                              (leash 逻辑不变 — 怪本来就在自己房间)
```

- **IDLE**：无怪 / 已清房 / Boss 房非战斗阶段。门组 = OPEN（玩家自由开闭）。
- **ARMED → LOCKED**：玩家完全进入（中心 tile 在房间内部，不在门 tile）→ 尝试关闭房间全部门组。
- **LOCKED → CLEARED**：房内 `is_alive` 计数归零 → 门组 OPEN + FOV 重算 + `ROOM_CLEAR` 事件（掉落/表现钩子挂此处）。
- **CLEARED**：本层内永久（不再武装）；进层重置。

### 8.3 边界情况清单（设计必须逐条回答，编码前逐条验收）

| # | 边界 | 规则 |
|---|------|------|
| E1 | 关门瞬间玩家/怪 rect 与门 tile 重叠 | **延迟关门**：ARMED 下每 tick 检测重叠，无重叠才落锁（<1s 窗口；防卡体） |
| E2 | 锁定瞬间有怪在门外（追击玩家路过） | 锁定条件 = **房内怪存在且门外无本房怪**；本房怪在门外时暂缓落锁，其 leash 会让其返房后再锁（v0.9.35 行为天然配合） |
| E3 | 多门房（孔径修复后 2~5 门） | 全部门组同闭同开；任一门组不可安全关闭 → 整体暂缓（原子性） |
| E4 | Boss 房 (F5/10/15) | 照常武装锁定 — 与现有机制阶段（MECHANIC_PHASE）正交；F10 熔岩地形不受影响；Boss 死 = 清房开门 ✓ 与现有 `_activate_stairs` 流程衔接 |
| E5 | 特殊房 / 事件房（含 AMBUSH 四周刷怪） | 特殊房 trigger (E) 在房内 → 锁定不影响；AMBUSH 在房内刷怪 → 若玩家已在房内且房武装，刷出的怪计入清房判定 |
| E6 | 玩家死亡/The World 时停 | 死亡走现有流程（门状态随层重建）；时停只冻结世界，门逻辑自然停摆 |
| E7 | 楼梯房（rooms.back()） | 楼梯激活 = 清层，层清后无武装意义；`is_floor_cleared` 优先级高于任何 LOCKED |
| E8 | 击退/传送把玩家推出 LOCKED 房 | 门已 CLOSED = 碰撞墙 → 推不出去（击退已有撞墙 clamp，Q 系列修复覆盖）；传送类技能（暗影突袭）落点判定走 `is_walkable` → 不会落进墙/关门外 |

### 8.4 与现有 leash 的关系（重要正面结论）

v0.9.35 的 leash（巡逻 4.5 / 追击 8.0 / 掉血解束缚）与房间锁定**天然互补**：怪物锚点=出生点=房间内；锁定后房内怪不需要 leash 约束（跑不出门），房外怪被 E2 暂缓规则排除。EXPLORATION_AUDIT P5（leash 圆大于房间内切空间 2.4% 拟合）在锁定制下自动消解 — 怪被门物理封在房内，领地感由空间本身提供。

---

## 9. 焦点 7：Sim AI 如何处理 CLOSED Door

| 方案 | 内容 | 优劣 | 判定 |
|------|------|------|------|
| **S1（推荐）统一规则** | CLOSED 门对 sim 与玩家行为完全一致：sim BFS（`_bfs_toward/_bfs_away/_bfs_toward_room` 的 `_tile_rect_walkable`）把 CLOSED 门 tile 视为**可通行**（因为接触即开 R1），移动碰撞同样先开后行 | 零特判分支；sim 忠实体验玩家规则；确定性不受影响（门开合由确定性输入驱动）；500 局回归仍有效 | **推荐** |
| S2 sim 绕过 | sim 模式下门恒 OPEN、房间不武装 | sim 完全不锻炼新系统；"回归可比"是假象（实机节奏已变）；两套行为漂移 | 否决 |
| S3 sim 完整建模 | BFS 把 CLOSED 门当墙、显式寻路开门 | 过度工程：BFS 每 tick 重算本来就会跟上门态；当墙反而制造 sim 在 1 门房前的假性不可达 | 否决 |

- 实现落点：sim_ai.cpp 的 3 个 BFS + 真实移动路径，对 CLOSED 门 tile 给出 `可通行(先开)` 语义 — 预计 ≤10 行。
- 回归预期：门开合是确定性的（sim 输入确定性 ✓），胜率分布主要由"探索路径变保守/接战顺序变化"微移 — 500 局回归接受 6~10% 带内波动，超出则按 Q3 系列惯例调参。
- **清房判定/锁门在 sim 中照常运作**（sim 会经历 LOCKED → CLEAR 全流程），这样新系统的回归是真回归。

## 10. 焦点 8：Save v3→v4 是否真的需要保存 DoorState

**结论：不需要。SaveManager 零改动，版本停在 v3。**

证据链：
1. 存档时点只有两个：清层（`_activate_stairs` 流程，game_scene.cpp:1528）与 Esc 退 title（game_scene_input.cpp:66）— 均为"层边界"。
2. `SaveData` 无怪物字段（save_manager.h:10-33）→ 读档 = `enter_floor(seed)` 从种子重生楼层与全部怪物（game_scene.cpp:318）。
3. 因此**门状态的正确读档值 = 初始态（OPEN）**，与怪物重生语义完全一致；层内过程态（哪些房已清）随怪物重生自然归零。
4. `special_triggered/discovered` 之所以要存，是因为它们是跨层持久语义（B8 决策）；门状态没有跨层语义。
5. 唯一注意点：Esc 中途退出版保存"当前层 seed" → 重进后 LOCKED 房间解锁、房内怪重生 — 等价于"该层重来"，与现有怪物重生行为一致，**无新问题**（也意味着"锁血退出重打"的轻微可利用性，与现状一致，不新增）。

---

## 11. 焦点 9：EventBus / Room / Door / Combat 职责边界

### 11.1 所有权划分（设计提案）

| 组件 | 拥有 | 明确不拥有 |
|------|------|-----------|
| **GameMap** | tile 级门状态（door_state 字段）、`is_walkable`/`blocks_sight` 的门感知、门组开闭 API | 房间语义（不知道"房间"是什么） |
| **RoomManager**（新，~150 行，GameScene 持有） | 房间状态机（IDLE/ARMED/LOCKED/CLEARED）、房间↔门组映射（进层时从 `get_connections()` 构建）、锁定/清房判定、清房奖励钩子 | tile 碰撞/视线（查询 GameMap）、伤害计算 |
| **CombatSystem/GameSceneCombat** | 伤害/击杀/on_monster_killed（现状不动） | 门逻辑；清房时由 RoomManager 监听 `MONSTER_DIED` 驱动状态机，而非 Combat 反向调用 Room |
| **EventBus** | 新增 3 事件：`ROOM_ENTERED` / `ROOM_LOCKED` / `ROOM_CLEAR`（+可选 `DOOR_OPENED`） | 不携带门 tile 坐标逻辑（payload 仅 room_idx） |
| **PlayerController** | 移动碰撞（现状）；接触开门规则 R1 挂在 `can_move_to` 命中门处 | 不判断房间归属（由 RoomManager tick 查玩家 tile） |
| **SimAI** | BFS 的门语义（S1，≤10 行） | 不感知房间状态机（LOCKED 不改变 sim 决策输入 — 门开合照常发生即可） |
| **MinimapRenderer** | 只读 door_state 换色（SHUT 变体） | 同 Phase 3 原则：无第二套状态 |

### 11.2 数据流（每帧）

```
GameScene::tick
  ├─ PlayerController::tick ──(命中 CLOSED 门)──► GameMap::open_door_group → FOV 重算
  ├─ RoomManager::tick(player_tile, monsters)
  │    ├─ ARMED 判定 → 尝试 close (E1/E2/E3 通过) → ROOM_LOCKED 事件
  │    └─ LOCKED 中监听房内怪清零 → open + ROOM_CLEAR 事件 (掉落/演出钩子)
  └─ (FOV tile-cross 更新 — 现状不变, game_scene.cpp:954)
```

---

## 12. 焦点 10：对现有系统的影响面（文件级）

| 文件 | 影响 | 规模 |
|------|------|------|
| dungeon_generator.cpp | **A1 孔径修复通道**（generate 尾部：环墙缺口扫描→door 化/回填→连通性自检） | 中（~80 行 + 自检） |
| game_map.h/.cpp | Tile.door_state + blocks_sight/is_walkable 门感知 + open/close door group API | 中 |
| config.h | `FOV_RADIUS_DEFAULT` | 1 行 |
| floor_manager.h (+FloorConfig) | `fov_radius` 字段（0=默认） | 小 |
| game_scene.h/.cpp | FOV_RADIUS 引用切换；RoomManager 挂载与 tick；门开合触发 FOV 重算 | 小~中 |
| **新增** room_manager.h/.cpp | 状态机 + 映射 + 判定 | ~150 行 |
| event_types.h | +3 事件枚举 | 3 行 |
| sim_ai.cpp | BFS/移动的门语义（S1） | ≤10 行 |
| minimap.cpp/.h | SHUT 门色变体（只读） | ~6 行 |
| player_controller.cpp | `can_move_to` 接触开门（R1） | ~8 行 |
| save_manager | **零改动**（§10） | 0 |
| monster AI (ai.cpp) | **零改动**（leash 与锁门正交，§8.4） | 0 |
| tests | +seal 不变量、+door 语义单测、+room 状态机单测、fov_test 增门遮挡用例 | +3 组 |
| game_renderer / UI | LOCKED 房间边框提示（可选，复用 room_msg） | 可选 |

---

## 13. 实施分期建议（供审核 — 每批独立可交付、可回滚）

| 批次 | 内容 | 验证门 | 预估 |
|------|------|--------|------|
| **Batch 1 — 空间基座** | A1 孔径完整性（P0）+ seal 不变量测试 + 门状态机（OPEN/SHUT）+ R1 接触开门 + blocks_sight 门感知 + FOV 半径可配置 | seal 不变量 7 seeds 全过；39+N 测试全绿；**工具复测：隔门泄露 93.2%→<5%、密封率 0%→100%**；39 旧测试不回归 | 主战役 |
| **Batch 2 — Room Encounter** | RoomManager 状态机 + E1~E8 边界 + 3 事件 + LOCKED UI 提示 + sim S1 语义 | 状态机单测全绿；500 局 sim 回归 6~10% 带内；确定性对拍一致 | 中 |
| **Batch 3 — 扩展（可选拆分）** | LOCKED/SEALED 门 + Minimap 特殊房标记（P3 捆绑此处）+ 门 sprite/视觉打磨（P4） | 对应单测 + 实机确认 | 小 |

> Batch 1 独立成立：即使 Batch 2 缓行，孔径修复 + 关门挡视线本身已解决 P1（走廊 2.27 房 → ≈0），探索体验立即改善。Batch 2 在 Batch 1 之上是纯增量。

---

## 14. 风险登记册

| # | 风险 | 等级 | 缓解 |
|---|------|:---:|------|
| R1 | A1 回填墙破坏走廊连通 | 高发面 | 回填仅限"外侧为墙的死凹槽"；每 seed 回填后 BFS 全图连通自检，失败则该缺口改 door 化（降级策略内建） |
| R2 | 门数上升（2~5/房）后房间战斗"关门动画"频繁 | 中 | 关门无演出（状态切换即成），仅 LOCKED 有一次性边框/音效 |
| R3 | sim 胜率带移 | 中 | S1 保持行为连续；500 局回归按 Q3 惯例校准 |
| R4 | 房间战斗 + 多门房 = 玩家被 5 门房围困挫败感 | 中 | E3 原子关门；清房后全开；后续可试"清房只开玩家侧门"（数据驱动开关，默认关） |
| R5 | 接触开门与特殊房 E 键交互混淆 | 低 | 门无交互提示（自动开），特殊房保持"按 E"提示文案 |
| R6 | 孔径修复改变既有 seed 的地图布局 | 确定 | 布局必然变化（缺口被处理）→ 500 局基线需重新采集（reports/baseline_s*.json 刷新），确定性对拍在同版本内自洽即可 |

---

## 15. 验证计划

1. **单元**：door 语义矩阵逐格断言；R1 接触开门；状态机 E1~E8 逐条；seal 不变量（7 seeds + fuzz 20 seeds）。
2. **回归**：39 既有测试全绿（fov/dungeon_topology/dungeon_verify/minimap 均不得回退）。
3. **Sim**：500 局胜率带 6~10%；确定性对拍（同 seed 双进程逐字节）。
4. **量化工具**（已就位，gitignored）：audit2 复测 — 目标 `sealed=100% / leak@k1 <5% / corridor_rooms_seen <0.3`；半径曲线复测验证 §6/§7 结论。
5. **实机**：走廊悬念感 / 开门揭示节拍 / 锁定战斗挫败度 — 人工确认（沿用 §八 方法论）。

---

## 16. 待用户决策清单（审核本审计时请逐条表态）

| # | 决策点 | 推荐 | 备选 |
|---|--------|------|------|
| D1 | 孔径修复方案 | **A1**（修复通道 + 内建连通降级） | A2（carve 本体改造） |
| D2 | 探索期门默认态 | **默认 SHUT + R1 接触开启**（P1 的实际解） | 默认 OPEN 仅战斗关门（P1 不解决，不推荐） |
| D3 | sim 方案 | **S1 统一规则** | S2 绕过 / S3 建模 |
| D4 | FOV 半径 | 可配置 + 默认 8（per-floor 留 6~8 空间） | 默认 6 |
| D5 | 分期确认 | Batch 1 → 2 → 3（1 独立可交付） | 1+2 合并 |

---

*本审计零代码改动。测量工具：`build/tmp_exploration_audit.cpp`（第一轮：泄露基线）与 `build/tmp_exploration_audit2.cpp`（本轮：密封性/孔径/半径曲线/CLOSED 门对比），gitignored，方案实施后用于 §15.4 验收复测。*




