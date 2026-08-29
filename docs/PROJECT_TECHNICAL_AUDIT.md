# Roguelike C++ Project Technical Audit

> 日期: 2026-08-29 · 基线: `d89ac1f` (v1.3.0) · 模式: 只读审计，零源码改动
> 审计原则: 最小可维护架构优先。只有满足 A-I 判据（已致 bug / 高概率致 bug / 破坏一致性 / 破坏确定性 / 重复维护成本 / 生命周期问题 / 性能热点 / 阻碍资源管线 / 破坏职责边界）才建议修改，否则标记"当前可接受"。

## 1. Audit Scope

- 代码: `src/` 全部 31 个目录、297 源文件（重点精读 14 个核心文件约 8000 行）
- 测试: `tests/` 53 个 ctest 条目清单与覆盖映射
- 存档: Save v4 读写路径、向后兼容、seed 恢复
- 确定性: 全部 RNG 源点（git grep 全量枚举，非抽样）
- 生命周期: enter_floor / arena 进出 / 死亡重启 / save-load 全链路走读
- 明确排除: 美术资产、音频内容、叙事文案、AI 算法调优

## 2. Project Snapshot

| 维度 | 数据 |
|------|------|
| 最大文件 | game_scene.cpp 2606 行 / boss.cpp 1162 / game_renderer.cpp 838 |
| 测试 | 53/53 ctest 通过（本次审计实测: 0.84s，0 失败） |
| 构建 | 本次重建无 warning/error 输出 |
| 确定性 | 基建完备: CountingRng 掷骰计数、seed 贯穿、instance_id 键（simulation_test/mcts/sequence_e2e 锁定） |
| 存档 | v4（gold/key/RUN relic/seed），getV 兼容旧档 |
| 架构 | Manager/Controller 分层清晰，Data Driven (JSON) 落地中 |

**总体健康度: 良好（B+）**。确定性基建（CountingRng/instance_id 键/seed 贯穿）是同类项目少有的强度。主要债务集中在 **floor transition 状态残留**（3 个 P1 同根）与 **渲染层污染游戏 RNG**（1 个 P1）。

## 3. Architecture Map

```
main / SceneTree (实例生命周期 owner)
 └── GameScene ────────────────── 【协调者 + 部分状态拥有者】
      ├── World
      │    ├── GameMap            【状态拥有者】tiles/door_state/FOV/arena_objects/special_rooms
      │    ├── DungeonGenerator   【工具】seed 驱动一次性生成
      │    ├── RoomManager        【状态拥有者】encounter 状态机 (IDLE→ARMED→LOCKED→CLEARED)
      │    ├── ChallengeRoomController 【状态拥有者】挑战状态机 (9 相位)
      │    ├── SpecialRoom/Encounter/EventSystem 【工具+配置】
      │    └── FloorManager       【工具】怪物生成
      ├── Combat: CombatSystem(公式+buff+全局rng) / WeaponExecutor / CombatCoordinator
      │    └── RewardManager      【工具】统一发放 (不发概率)
      ├── Entities: Player(自身状态 owner) / Monster / Boss
      ├── AI: BTAgent/MCTS/SimAI/Mirror (sim 路径独立)
      ├── Progression: Inventory/Gold/Key/Relic(PersistenceScope FLOOR/RUN)
      ├── Render: GameRenderer / DoorRenderer / Minimap / VFXServer
      │    └── ArenaManager       Boss 房 DangerZone 生命周期 (owner=BossAI 读取)
      ├── Save: SaveManager (v4)
      └── Directors: GameFlow/BossSystem/Presentation/Camera... 【协调者】
```

职责边界总体符合项目既定原则（TeamCoordinator 只读、ArenaManager 只管 DangerZone、RewardManager 不掷概率）。例外见 §5/§7。

## 4. Module Responsibility Matrix

| 模块 | 类型 | 状态拥有? | 越界情况 |
|------|------|----------|---------|
| GameMap | 状态拥有者 | tiles+doors+FOV | 无 ✓ |
| RoomManager | 状态拥有者 | encounter 状态机 | 无 ✓（回调解耦正确） |
| ChallengeRoomController | 状态拥有者 | 挑战相位 | GameScene 直接调 `set_phase_for_test` 设生产相位（见 ENT-002） |
| GameScene | 协调者 | world_mode/arena 暂存/portal 演出 | 越界见 §7，当前可接受但需冻结 |
| CombatSystem | 公式+工具 | 全局 CountingRng | ✓（含掷骰计数诊断） |
| GameRenderer | 工具 | 无 | **在 game_scene 渲染路径消费全局 rng()**（RNG-001） |
| RewardManager | 工具 | 无 | ✓ |
| SaveManager | 工具 | 存档文件 | ✓（arena 内存档已在输入层封堵） |

## 5. State Ownership Map

| 状态 | 创建 | 修改 | 读取 | 重置 | 判定 |
|------|------|------|------|------|------|
| DoorState | DungeonGenerator | GameMap API (set_door_state/组 API) | 全系统 | 随 map 重建 | ✓ 唯一 owner |
| Encounter 状态机 | RoomManager::build | RoomManager::tick | GameScene | 仅 build（boss 层**不重建**→LIFE-002） | ⚠ |
| Challenge 相位 | ChallengeRoomController | Controller + GameScene(键/portal) | GameScene/renderer | 仅 exit_challenge_arena（**enter_floor 不重置**→LIFE-001） | ⚠ |
| WorldMode | GameScene | enter/exit_challenge_arena | 多处 | exit 时 DUNGEON；死亡在 arena 时随场景销毁重建恢复 | ✓（重启走 SceneTree 新实例，已验证） |
| Arena 暂存地图/怪物 | GameScene | enter/exit_challenge_arena | 渲染 | exit 时 restore+reset | ✓ |
| current_floor | GameScene（真实 owner） | enter_floor | 全系统 | enter_floor | ⚠ Player 上有死副本（STATE-001） |
| FOV/explored | GameMap | update_fov | 渲染/AI | reset_visibility | ⚠ arena 返回误清 explored（LIFE-003） |
| gold/key/relics | Player | Player API | HUD/save | 存档链路 | ✓ |
| 全局 rng | combat_system.cpp | seed_rng | 战斗+**渲染屏震**(RNG-001) | enter_floor 换层不重置（跨层延续流） | ⚠ |

## 6. Lifecycle Audit

走读链路: 启动→Run→enter_floor→探索→战斗→SpecialRoom→Portal→Arena→返回→清层→Save/Load→死亡→重启→退出。

**enter_floor()（game_scene.cpp ~260-430）重置清单核对**:
✓ monsters/active_effects/pending_damage/room_msg/boss arena/_unstuck maps/FLOOR relics/event 状态/FOV 缓存/_boss_last_known
✗ `_challenge.reset()` —— **缺失**（仅 exit_challenge_arena:2818 调用）
✗ `_room_mgr` 重建 —— 仅非 Boss 分支调用 build；Boss 层沿用上一层房间（LIFE-002）
✗ `_arena_map/_arena_monsters` 清理 —— 依赖场景重建兜底

**已验证安全的点**:
- 死亡/重开: SceneTree `_root.reset()` 重建整个 GameScene 实例，成员残留不跨 Run（v0.9.19 EventBus 悬挂订阅修复仍在位）
- Arena 内存档: `is_save_blocked()` 封 COMBAT 系相位 + 输入层 ESC 跳过存档（3I），无"存档于 arena 坐标→读档卡墙"路径
- Save v4: seed 保存/恢复（`getV("seed",0)` 旧档回退随机——语义上等价"新楼层"）、gold/key/RUN relic 均在读写两侧
- 传送 fade 期间输入: fade 计时结束才落位+ARMED，无半帧状态

## 7. GameScene Audit

职责分类（基于 game_scene.h 成员清单，2606 行 + input/combat/interaction 三个 partial 文件）:

1. **合理 Scene Coordination**（保留）: 状态机 GameState、tick 编排、Director 调度、事件演出触发
2. **偏 Controller 逻辑**（当前可接受，冻结即可）: world_mode 切换、arena 地图交换、portal fade 计时、challenge choice UI
3. **渲染辅助**: `_draw_map/_draw_entities` 轻量入口，主渲染在 GameRenderer ✓
4. **Sim/Replay 统计**: `_sim_*` 成员群 —— 与 gameplay 同层但通过 `return` 短路隔离 ✓

判定: **不建议拆分**。2606 行确实大，但 partial 类（Input/Combat/Interaction）已实现物理分治；五重 `friend` 使全部成员对 4 个辅助类裸露，边界靠约定——这是唯一实质风险（ARCH-001，P2 观察）。满足"职责明显混乱+已造成维护困难"的证据不足，故维持现状。

## 8. Player / Entity Audit

**Player 状态容器**（player.h，114 行）: CombatStats/Inventory/Skills/Relics/Gold/Key/XP/Combo/Weapon/Element —— 全部有明确 API 与 owner，规模合理，**不构成"过大容器"**，无需 Component 化。

- `Player::current_floor`（player.h:59，注释"临时方案"）: git grep 全量核对后 **无任何读点**——GameScene::current_floor 才是唯一有效 owner，save 数据独立持有。判定: 死字段，删除即最小修复（STATE-001，P3）。**不需要迁移，不需要 ECS。**
- Entity center: `sync_rect()` 以 collision_size 居中构建 rect，`entity_center_test` 已锁定行为 ✓。散落的 tile 中心算式（`tile*32+16` / `TILE_SIZE/2` / `/32`）共 15 处（bt_agent/sim_ai/renderer/interaction/challenge COMBAT 计数）——同一概念四种写法，ENT-001（P3，建议统一为 `tile_center()`/`map->tile_size` 引用，非紧急）。
- 未发现 32×32 假设引起的**错位 bug**（entity_center_test 已覆盖 hitbox 与视觉一致性）。

## 9. Door / Room / Portal Audit

**DoorState Truth Table**（源: game_map.h:30-49 注释 + 实现核对）:

| State | Walkable | Blocks Sight | E 开 | R1 接触开 | RoomManager | SimAI |
|-------|----------|--------------|------|-----------|-------------|-------|
| OPEN | ✓ | ✗ | — | — | 开门目标 | 通行 ✓ |
| CLOSED | ✗ | ✓ | ✓ | ✓(try_open_door_toward 仅 CLOSED) | — | 阻挡 ✓ |
| LOCKED | ✗ | ✓ | ✗ | ✗(仅检查 CLOSED，correct) | 封门用此态 ✓ | 阻挡 ✓ |
| SEALED | ✗ | ✓ | ✗ | ✗ | 未使用(预留) | 阻挡 ✓ |

语义一致 ✓（RoomManager `_try_lock` 用 `lock_room_doors`=LOCKED 而非 CLOSED，接触开门只认 CLOSED——两处最易错点均正确）。

**Room Encounter × Challenge Portal 交叉**:
- challenge ARMED 分支调用 `lock_room_doors({})`/`open_room_doors({})`（992/996）——空向量循环零次**返回 true 什么都不做**。Portal 流(3I)下挑战发生在 arena（地图交换），地牢门无需锁，故当前**无实际 bug**；但这段+`try_activate`/UNLOCKED 相位构成不可达的 legacy 双路径（STATE-002，P2）
- Portal 状态残留: `setup_portal` 每层重设 → PORTAL_ACTIVE 良性覆盖；CLEARED 残留仅影响 pulse 演出，portal 坐标每层重设 → 无玩法影响
- WorldMode 错误切换: exit/enter_challenge_arena 有幂等守卫（`if (_world_mode != ...) return`）✓

## 10. Determinism Audit

**Determinism Risk List**:

| 级别 | 编号 | 位置 | 问题 |
|------|------|------|------|
| P1 | RNG-001 | game_scene.cpp:1905-1906 | **渲染屏震消费全局 rng()**（每帧 2 次 draws）。渲染调用序改变→战斗掷骰流移位。当前 Sim headless 不受影响，但 replay-hash 校验、未来 sim-with-render、任何渲染序改动都会使同输入不同结果 |
| P2 | RNG-002 | boss.cpp:1100 | `seed ? seed : random_device{}`——旧档 seed=0 时 BossType 进程随机（现流程 enter_floor 先保证 _dungeon_seed≠0，为防御死代码，但边界在） |
| P2 | RNG-003 | encounter.cpp:76,86 | 裸 `rand()` 选取 encounter——**已核实调用点仅判空丢弃结果**，当前无玩法影响（潜在雷） |
| Safe | — | game_renderer.cpp:89-90 | `rand()` 纯视觉（未播种 C rand，单线程） |
| Safe | — | combat_system.h CountingRng | 全局唯一战斗流，seed_rng 贯穿 enter_floor/replay，掷骰计数可诊断 ✓ |
| Safe | — | challenge_room._deterministic_seed | dungeon_seed⊕room⊕wave 派生，challenge_room_test 锁定 ✓ |
| Safe | — | audio_server/wave_synth | mt19937(42) 固定种，独立流 |
| Safe | — | event_system | 显式传 rng 参数，无全局污染 ✓ |

结论: 无 P0。RNG-001 是唯一越层写入点；修掉后确定性管线（seed→map→combat→challenge）全绿。

## 11. Rendering / UI Audit

| 检查点 | 结论 |
|--------|------|
| 世界/屏幕坐标空间 | GameRenderer 统一以 cam 偏移交换，HUD 使用独立屏幕坐标 ✓ |
| Screen Shake → HUD | 屏震仅偏移 `_cam_x/_cam_y`（1902-1909），HUD 在 cam 恢复后绘制，不受影响 ✓（但见 RNG-001: 消耗 rng()） |
| RenderTexture 生命周期 | 未发现跨帧失效引用；DoorRenderer 纹理 `load once, reuse` ✓ |
| Minimap | 只读 GameMap，无第二套探索状态 ✓（探索态被误清见 LIFE-003） |
| Portal Fade | 计时器驱动，fade 结束才落位，无半帧渲染 ✓ |
| Door Animation | DoorRenderer::update 独立 tick ✓ |
| 视觉 RNG | game_renderer.cpp 用独立 `rand()`（RNG-004, Safe），与 game_scene 屏震混用全局 rng() 形成不一致——修复方向见 RNG-001 |

无渲染系统重写需求。

## 12. Performance Risk Audit

全部为 **【Potential】**（无 profiler 数据，按规则不下"严重"结论）:

| 位置 | 模式 | 评估 |
|------|------|------|
| game_map.update_fov | 全图 is_visible 清扫 + 360 射线×半径步进 | 每次跨 tile 才调用（有 tile 变化守卫）+ arena 进入时——按当前地图尺寸可接受，Potential |
| update_boss_fov | 同上双份 | 同上 |
| RoomManager.tick | 仅激活房间扫描，IDLE 零开销 | 设计即防全图扫描 ✓ |
| Challenge COMBAT | 每帧 O(怪数) 计数 | 可接受 |
| game_map.draw | 逐 tile 绘制 | Potential——如帧率劣化，优先怀疑此处（可见区裁剪未核实），需 Measure 后再动 |
| get_special_room_at | O(房间数) 线性 | 调用频度低，可接受 |
| 怪物查询 | 每帧数次 O(n) 向量遍历 | n 小，可接受 |

**结论: 当前无实测热点。任何性能改动必须先 profiler（Measure），禁止凭代码样式优化。**

## 13. Test Coverage Audit

**Test Coverage Risk Matrix**（53 ctest 基础上）:

| System | Existing Tests | Critical Missing Test | Risk |
|--------|----------------|----------------------|------|
| Save v4 | save_v4_test, save_test | dungeon_seed 往返 roundtrip 显式断言 | 中 |
| PersistenceScope | relic_persistence_test | — | 低 |
| DoorState | door_interact/door_seal/door_renderer_test | LOCKED/E-负向 + truth-table 参数化 | 低 |
| RoomManager | room_encounter_test | **Boss 层 stale 不重建回归**（LIFE-002） | **高** |
| ChallengeRoomController | challenge_room_test | **跨层 reset 残留回归**（LIFE-001） | **高** |
| Portal/WorldMode | challenge_portal_test | arena 返回后 explored 保留断言（LIFE-003） | 中 |
| Gold/Key/Sell | gold/key/inventory_sell_ui_test | — | 低 |
| Gamble Room | gamble_room_test | — | 低 |
| Floor Transition | sequence_e2e_test(部分) | enter_floor 统一重置契约测试 | **高** |
| Reward | reward_manager_test | — | 低 |
| 渲染确定性 | — | 屏震不消耗游戏 rng 断言（RNG-001） | 中 |

模式: 现有测试多为单元级 happy-path + 关键负向（质量尚可），**缺口集中在"跨系统生命周期残留"——恰好是本次发现的 3 个 P1 所在**。

## 14. Technical Debt List

### P0（无）

无数据损坏/崩溃/存档损坏级问题。is_save_blocked 封堵了最危险的"战斗中存档"路径。

### P1

**【LIFE-001】Challenge Controller 状态跨层残留**
- Priority: P1
- Location: `game_scene.cpp enter_floor()`（重置块 ~280-313）; `challenge_room.cpp reset()`; 唯一 reset 调用点 `game_scene.cpp:2818 (exit_challenge_arena)`
- Current Behavior: enter_floor 重置块不调用 `_challenge.reset()`。相位于换层后残留: PORTAL_ACTIVE/UNLOCKED/CLEARED 均可带入新层
- Evidence: `git grep _challenge.reset` 仅 1 处（2818）; enter_floor 280-430 行无 reset; challenge_room.cpp:43 `on_player_entered` 依赖 UNLOCKED→ARMED，而 setup_portal 每层强设 PORTAL_ACTIVE，两相位语义在新层交叠
- Risk: 满足判据 B（高概率致未来 bug）+ F（生命周期）。残留 UNLOCKED 时进新层挑战房 → on_player_entered 直接 ARMED = 免钥匙挑战；残留相位还会使 980-998 的 tick 分支在新层空跑
- Impact: Gameplay（经济/挑战门槛）· Save（is_save_blocked 判定依据相位）· Maintainability
- Recommended Fix: enter_floor 重置块加一行 `_challenge.reset();`（在 setup_portal 之前）。不新增状态、不改相位机
- Do Not Do: 不要重新设计 ChallengePhase 状态机、不要把相位迁入 RoomManager、不要加 PERMANENT scope
- Estimated Scope: Small

**【LIFE-002】RoomManager 在 Boss 层沿用上一层房间数据**
- Priority: P1
- Location: `game_scene.cpp enter_floor()` 393（build 仅在非 Boss 分支）; `game_scene.cpp:1070`（tick 无 Boss 层守卫）; `room_manager.h`
- Current Behavior: 上一层（如 F4）build 的 `_rooms` 矩形/门组在 F5（Boss 层）继续生效。Boss 战期间 state==PLAYING → tick 活跃，玩家+Boss 落入旧矩形即 ARMED
- Evidence: build 调用点仅 393（else 分支）; enter_floor 重置块无 `_room_mgr` 清理; update() 518 行仅挡非 PLAYING
- Risk: 判据 B+F。旧门组坐标在新地图 lock_room_doors 部分失败 → `_try_lock` 卡在 ARMED（92 行 return 不回退状态）；极端布局下错误封门
- Impact: Gameplay（Boss 战门状态）· AI（Boss 卡位判定）
- Recommended Fix: 重置块加 `_room_mgr` 清理（或 Boss 分支也调用 build，is_boss_floor 参数已是现成语义）
- Do Not Do: 不要把 RoomManager 合入 ChallengeRoomController、不要改为每帧搜索门组
- Estimated Scope: Small

**【LIFE-003】退出 Challenge Arena 清空当前层探索进度**
- Priority: P1
- Location: `game_scene.cpp exit_challenge_arena()` 2810; `game_map.cpp reset_visibility()` 169-175
- Current Behavior: 返回地牢调用 `game_map->reset_visibility()` → is_explored 全清 → 小地图/探索进度丢失
- Evidence: reset_visibility 实现同时清 is_visible 与 is_explored（173 行）; 而 update_fov 本身每帧已清 is_visible（179-181），reset_visibility 在此调用点唯一实际效果就是抹掉 explored
- Risk: 判据 A（已是 bug）：玩家探索成果被 arena 往返静默吞掉；与 enter_floor:329 的"新层全清"意图共用同一 API，语义混用
- Impact: Gameplay（探索/小地图）· Render（minimap 显示）
- Recommended Fix: 删除 exit_challenge_arena:2810 的 reset_visibility 调用（update_fov 已承担可见性清理）；或在 GameMap 增加 clear_visible_only 并替换
- Do Not Do: 不要引入"探索快照/回滚"机制、不要缓存 explored 副本
- Estimated Scope: Small

**【RNG-001】渲染屏震消费全局确定性 rng()**
- Priority: P1
- Location: `game_scene.cpp:1905-1906`
- Current Behavior: 屏震偏移每次用 `rng() % 100` ×2 —— 从战斗用 CountingRng 全局流抽取
- Evidence: `rng` 为 combat_system.h:28 `extern CountingRng`；渲染路径调用序直接改变后续战斗掷骰
- Risk: 判据 D（破坏确定性）——渲染层写游戏 RNG 流。当前 Sim headless 掩盖了它，但 replay-hash 校验、sim-with-render、任何渲染顺序调整都会引入"同输入不同结果"
- Impact: Sim · Replay · AI（mirror 训练数据一致性）
- Recommended Fix: 屏震改用视觉专用源（本地 static mt19937 或 rand()，与 game_renderer.cpp 现有视觉 RNG 一致）。不动 CountingRng 语义
- Do Not Do: 不要给 CountingRng 加双流/分支、不要把屏震改成基于 game_time 的纯函数再回头动战斗流
- Estimated Scope: Small

### P2

**【STATE-002】Challenge 地牢 legacy 双路径（空门组 no-op）**
- Priority: P2 · Location: `game_scene.cpp:990-997, 1080-1088`; `challenge_room.cpp:55-63 (try_activate)`
- Current Behavior: `lock_room_doors({})`/`open_room_doors({})` 空向量 = 静默 no-op；try_activate/UNLOCKED 相位在 Portal 流下不可达
- Evidence: 空向量循环零次返回 true（game_map.cpp:141-147）；try_activate 无调用点
- Risk: 判据 I（边界混淆）。未来有人把 `lock_room_doors({})` 当真封门用
- Impact: Maintainability
- Recommended Fix: 删除地牢挑战 legacy 分支，或替换为真实挑战房门组；与 LIFE-001 同批处理
- Do Not Do: 不要保留双路径"以防万一"
- Estimated Scope: Small

**【RNG-002】boss_type_for_floor 的 random_device 回退**
- Priority: P2 · Location: `boss.cpp:1100`
- Current Behavior: seed==0 时进程随机。现流程 _dungeon_seed 已保证非 0，属防御边界
- Evidence: 1100 行三元；save_manager.h dungeon_seed 默认 0（旧档）
- Risk: 判据 D 边缘。旧档+重开路径若传入 0 → Boss 不可复现
- Impact: Sim · Replay
- Recommended Fix: `seed ? seed : 1u`（保确定性兜底）
- Do Not Do: 不要把 Boss 选择迁入全局 rng 流（会改变现有 seed 行为）
- Estimated Scope: Small

**【ARCH-001】GameScene 五重 friend + 全成员共享**
- Priority: P2（观察项）· Location: `game_scene.h:95-99`
- Current Behavior: Input/Combat/Interaction/PlayerController/GameFlowDirector 以 friend 访问全部成员
- Risk: 判据 I——边界靠约定。当前无实际 bug，未达拆分门槛
- Impact: Maintainability
- Recommended Fix: 冻结成员数，新逻辑一律入 Controller，friend 清单不再增加
- Do Not Do: 不要为"降 friend"做接口化大改
- Estimated Scope: —

**【TEST-001】生命周期残留零回归覆盖**
- Priority: P2 · Location: `tests/`
- Current Behavior: 无 enter_floor 契约测试、无 Boss 层 RoomManager 测试、无 arena 返回 explored 断言
- Risk: 本次 3 个 P1 的同类回归将再次漏网
- Impact: Maintainability
- Recommended Fix: 新增 floor_lifecycle_test: ①残留相位 → enter_floor → challenge==INACTIVE ②Boss 层后 room_mgr 为 Boss 层房间 ③enter/exit arena → is_explored 保留
- Do Not Do: 不要做全系统 e2e 大测试
- Estimated Scope: Medium

### P3

**【STATE-001】Player::current_floor 死字段**
- Priority: P3 · Location: `player.h:59` · Current Behavior: 全 src 无读点（git grep 全量核对），真实 owner 为 GameScene::current_floor + SaveData · Risk: 新代码误读得到 stale 值（判据 B 弱） · Recommended Fix: 删除字段 · Do Not Do: 不要"补同步"维持双份 · Estimated Scope: Small

**【ENT-001】tile 中心算式 4 种写法共 15 处**
- Priority: P3 · Location: `bt_agent.cpp:67-68`、`sim_ai.cpp:268-269/522-554`、`game_scene.cpp:1521-1533/2578`、`game_renderer.cpp:598-599`、`interaction_handler.cpp:42-43`、`challenge_room.cpp:118(/32)` · Current Behavior: `tile*32+16` / `TILE_SIZE/2` / `rect/32` 混用 · Risk: 判据 E——改 TILE_SIZE 或 tile 中心语义时 15 处漏改 · Recommended Fix: 提供 `map->tile_center(tx,ty)` 单一入口逐步替换 · Do Not Do: 不要动 entity rect/sync_rect 本身（entity_center_test 已锁定） · Estimated Scope: Small

**【RNG-003】encounter.cpp 裸 rand()**
- Priority: P3 · Location: `encounter.cpp:76,86` · Current Behavior: 选取结果当前被调用点丢弃（仅判空），无玩法影响 · Risk: 潜在雷——未来复用即引入非确定 · Recommended Fix: 改传入 rng 或删除死函数 · Estimated Scope: Small

**【ENT-002】生产代码使用 set_phase_for_test**
- Priority: P3 · Location: `game_scene.cpp:1016` · Current Behavior: 测试专用 setter 在 fade 落位路径使用 · Recommended Fix: 更名 force_armed() 或注明为演出完成回调 · Estimated Scope: Small

**【TEST-002】DoorState truth-table 参数化缺失**
- Priority: P3 · Location: `tests/door_*` · Recommended Fix: 四态×(walkable/sight/E/R1) 参数表断言 · Estimated Scope: Small

### P4（暂不处理）

- game_renderer.cpp 视觉 rand() 未播种——纯视觉、单线程、不进游戏流
- game_scene.cpp 部分函数偏长——partial 类已分治，无实际维护事故
- CombatSystem/MonsterAI 未 Component 化——不满足 A-I 任一判据
- FOV 360 射线固定分辨率——当前尺寸可接受，性能改动需 profiler 前置
- SaveManager fprintf/getV 文本格式——v4 稳定，格式迁移是独立大决策

## 15. Recommended Next Batches

> 每个 Batch 独立、小范围、可测试、可回滚、不混合无关问题。

**Batch G9.2 — 生命周期残留修复（3×P1 生命周期项）**
- 内容: enter_floor 补 `_challenge.reset()` + `_room_mgr` 重置（LIFE-001/002）；删 exit_challenge_arena 的 reset_visibility 调用（LIFE-003）
- 测试: floor_lifecycle_test 三断言（TEST-001 前半）
- 回滚: 全部为单行级增删，git revert 即可

**Batch G9.3 — RNG 边界修复（1×P1 + 1×P2）**
- 内容: 屏震改视觉独立 RNG（RNG-001）；boss_type seed=0 兜底 1（RNG-002）
- 测试: 用 CountingRng::draws 断言屏震不消耗战斗流
- 回滚: 独立提交，可单独 revert

**Batch G9.4 — 测试补强（TEST-001 收尾 + TEST-002）**
- 内容: lifecycle 补 Boss 层与 save-seed roundtrip 断言；DoorState truth-table 参数化
- 依赖: G9.2 合入后编写，锁定新基线

**Batch G9.5（可选 P3 清理）**
- STATE-001 删死字段、ENT-002 更名、RNG-003、STATE-002 删 legacy 分支——一次一个 P3，各带测试；ENT-001（tile_center 统一）单独成批，涉及 AI/Sim 需跑全套 sim 确定性测试

## 16. Explicitly Deferred Work

以下事项**明确不做**，除非未来触发 A-I 判据:

1. GameScene 拆分/接口化（ARCH-001 仅冻结）
2. Player Component 化 / ECS
3. DoorState/RoomManager/Challenge 状态机重设计
4. PersistenceScope 扩展 PERMANENT
5. 渲染系统重写、RenderTexture 管线重构
6. 任何无 profiler 数据支撑的性能优化
7. Save 格式 v5 迁移

---

## Review Gate — 审计总结（等待 Review，不开始任何修复）

**1. 总体健康度: 良好（B+）**
确定性基建、Manager 边界、DoorState 语义、经济/存档链路均处于同规模项目的高水准；53/53 测试通过；无 P0。

**2. 最严重的 3 个问题**
1. **LIFE-001/002/003（同根: enter_floor 重置不完整 + reset_visibility 语义混用）**——challenge 相位残留、Boss 层 RoomManager 沿用旧房、arena 返回清探索。最小修复合计 ~4 行 + 3 个回归断言
2. **RNG-001（屏震消费战斗 rng）**——唯一跨层确定性污染点，被 Sim headless 暂时掩盖
3. **TEST-001（生命周期零回归覆盖）**——上述问题曾漏网、将再漏网

**3. 最值得立即处理的 Batch: G9.2**（生命周期残留，Small，回滚成本≈0，一次消除 3 个 P1）

**4. 当前不要修改的系统**
- CombatSystem / CountingRng 语义（除 G9.3 明确列出的两处边界）
- ChallengeRoomController 状态机（只加 reset 调用，不动相位机）
- RoomManager 状态机（只重置数据，不改 tick 逻辑）
- DoorState 语义与 GameMap 门 API（truth table 已验证一致）
- Sim / RL / Mirror 全链路（本次审计未发现其内部问题）
- Save v4 格式（含 getV 兼容逻辑）
- 渲染系统整体（仅 G9.3 屏震单点）

**git 工作区终态**: 仅新增 `docs/PROJECT_TECHNICAL_AUDIT.md`（本文件）+ 既有未跟踪 `.clinerules/`；零源码/测试/CMake/资源改动。



