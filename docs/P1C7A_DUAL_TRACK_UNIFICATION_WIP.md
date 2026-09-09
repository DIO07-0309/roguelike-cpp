# P1-C7-A · 双轨判定统一 — 进行中交接 (Work In Progress)

> 日期: 2026-09-09 (第 4 次会话: 聚合判决 — 迁移第三次负回归, 已回退) · 状态: **实验回退到干净基线**
> (e89b793 基线可复现: af=6.25/dmg=1746.2 s3串行20局 精确一致)
> 方案: 空手 (fist_basic) 从 legacy `Player::can_attack` (0.5s) 轨迁移到
> WeaponExecutor/WeaponComponent 轨 (数据驱动), fist stage 调至 range=1.5
> (48px 手感保持) + recovery=0.35 (≈0.5s 出手间隔)。

## 🔴 第 4 次会话结论 (聚合判决 — 迁移第三次失败, 负回归系统性坐实)

**5 种子×100 局聚合 (可判读维度, 补上了 WIP 缺的最后一块证据):
v3 全面负回归 → 已回退。迁移路线三连败 (v1/v2/v3), 判决: 0.35s 出手节奏
在串行混沌流中是系统性劣化, 不是方差。**

### v3 聚合数据 (5 种子 × 100 局, reports/p1c7a/p1c7a_v3_s{3,7,11,19,23}.out)

| 指标 | 基线 (p1c4 500局) | v3 (500局) | 变化 |
|---|---|---|---|
| avg_floor | 1.60 | **1.30** | -19% |
| avg dmg dealt | 196 | **99** | **-49%** |
| s7 极端形态 | — | dealt=2.1/局 (近零输出速死海) | — |
| Boss kill F5+ | — | s7=0%, s23 乏力 | — |

s3 单独对照: 基线 2.05/349 vs v3 1.19/95.6 — 与聚合方向一致。

### 本会话的三个诊断增量 (全部探针已移除, 证据在 .out)

1. **chase 再平衡实验 (一次, 负回归, 已回退)**: WIP 候选方案
   "3 格内未死怪 → BFS 追击 0.65 分压过搜刮 0.6" 实测 s3 串行 20 局
   af 6.25→2.10 (run0-3 起飞 F6-9.5 后 run4-19 崩塌)。探针实锤原因:
   **move 0.65 > attack 分 (d>32px 时 atk≤0.42) → AI 永远不选 attack**
   (C7AV3: calls 从 306 → 0/局)。教训: `_evaluate_move` 加分项必须与
   `_evaluate_attack` 分数量级联审 — 两者是同一次 best_action 里的竞争者。
2. **"run2+ 空手零出手"之谜解开 (果非因)**: 移除 chase 后 run0 calls=306
   (链路健康) 但 run2+ calls=0 复现。GATE2 探针 (handle_input 入口 600 帧
   采样) 显示 run1-4 全部 **GATE2=0 采样** — 即这些局整个生命周期 < 600 帧
   (10s) 速死。calls=0 是"没活到出手"的果, 不是"AI 不想打"的因。
   C7ADEC 探针同时证明决策层健康: atk>0 样本 211/288, 贴脸时 atk 0.4-0.63
   稳定压过 mv 0.4 → 决策层在选 attack, 是执行层没来得及。
3. **速死链**: run1+ 开局 10-40s 内 F1/F2 死 (兽人 4-5/击 + 毒 3-9/tick +
   围殴), 与 v2 的 C7BFINAL 剖面一致。0.35s 节奏在单局批 (每批只有 run0)
   无此形态 — 串行批的 rng 混沌传播让 run1+ 落入更差的 F1 开局分布。

### 为什么三次尝试都翻在同一处 (机制总结)

- 0.35s 出手比 0.5s 快 30% → 空手期 DPS 账面更高 → 但 F1 生存瓶颈不是
  DPS 而是站位/集火顺序: 更快杀怪 = 更早切换目标 = 出圈窗口更长 =
  搜刮/拾取分支在空隙赢得决策 (0.6-0.7 分) → 战斗-资源震荡 → 被未死怪
  追杀 + 毒叠加 → 速死。**节奏变化改变了 AI 的决策相位, 而决策权重表
  (Q3.15 调的) 是按 0.5s 节奏标定的。**
- 这与 P1-C5/C6 的负回归同构: 出手时序、判定圈、搜刮权重三者是耦合系统,
  单点改动必然翻车。**统一双轨的前置条件是先重构决策权重表, 而不是先迁轨。**

## 🔴 第 3 次会话结论 (P1-C7-B 泄漏排查 — 假说全灭, 真因定位)

**"跨局状态泄漏"假说被系统证伪。真因 = 迁移的出手节奏变化触发了
"战斗-搜刮时序再平衡"，在串行批的混沌流中演化为更差结局。**

### 排查记录 (每项一探针, 全部已验证)

| # | 假说 | 探针 | 判决 |
|---|---|---|---|
| 1 | 镜像冻结跨局/跨层残留 (Q3.9 reset_run 放进 reset() 但全仓零调用) | new_game/enter_floor 补调 reset/clear_mirror_freeze + 行为对比 | **证伪** — 修复后 30 局日志与修前仅差时间戳 (逐字节) |
| 2 | UI 状态跨局残留 (event/dialogue/quest/challenge/inventory) | enter_floor 全 UI 标志快照 | **证伪** — 全部层进入时 0 |
| 3 | GameState 非 PLAYING 跨局残留 | handle_input 入口 state 采样 (660 帧) | **证伪** — 99% PLAYING + 4 次 Boss 入场 (正常) |
| 4 | AI 意图 attack 但输入门吞 (frozen/UI/is_action 缓存) | 350 行查询失败 + AI 意图对照探针 | **证伪** — 零失败样本 |
| 5 | executor can_attack 门异常 (recovery/special/fatigue) | 拦截原因分类计数 | **证伪** — recovery 拦 68% 属正常 0.35s 节奏; special=203 是 spear 局尾累计 |

### 真因 (C7BFINAL 终点决策探针, run2 全程剖面)

run2 的 AI 最终决策序列: 攻击有效 (nhp 30→15→3→0, 伤害链健康) →
**t=14.6 起再无 attack — AI 在怪 15 血未清时切换目标/启动搜刮, nd 增大
远离怪 → 被 15 血怪追杀 → 毒磨死**。

**机制**: v2 出手 0.35s/击 (基线 0.5s) → 杀怪更快 → 目标切换更早 →
出圈窗口更长 → `_evaluate_move` 搜刮分支 (0.6) 在追击空隙赢得决策 →
战斗-搜刮震荡形态。**与 P1-C6 的教训同构**: 出手时序与 Q3.15 风筝/
搜刮平衡深度耦合, 单点改节奏必然翻车。

**同批附带发现 (仍有效, 值得单独修)**:
- `BossSystemDirector::reset()` 设计注释"新楼层开始时调用"但**全仓零调用**
  — replay_mem/evolution/arena 等 boss 状态跨层跨局全残留。本批曾在
  new_game/enter_floor 补调 (已回退, 因 replay_mem 语义需拆分: 局内跨层
  须持久 vs 跨局须清)。**建议 P1-C7-C: reset 语义拆分 + 补调用点**
- `C7BGATE` 探针的 special=203 提醒: WeaponSpecialState 激活中局结束
  (spear 10 连击 tick 中死亡) 的跨局残留无法从 Player 重建角度发生
  (新 Player runtime 干净) — 已排除, 但 spear 局尾的 special 拦截期
  (放行停止) 值得在 P1-C7-A 复审时用 per-run 探针盯一次

### P1-C7-A 下一步 (第 5 会话入口 — 路线改道)

1. **停止"参数级迁移"尝试** — 三连败 (v1 range对齐 / v2 0.35s 节奏 /
   v3 0.35s+再平衡) 证明: 在不动决策权重表的前提下迁移不可能不翻车。
2. **新路线候选 (推荐 B)**:
   - **A. 决策权重表先行重构**: 把 `_evaluate_attack`/`_evaluate_move`
     的分数量级统一 (attack 上限 1.0 vs move 上限 1.4 的竞争关系重审),
     然后迁移 (工作量: 一次 500 局平衡回归 + 可能多轮迭代, 风险高)
   - **B. 节奏保持迁移**: fist recovery=0.35 改为 **0.5** (与 legacy
     ATTACK_COOLDOWN 完全等值, 唯一差异只剩判定几何从 find_attack_target
     48px 圆 → HitShape CIRCLE 1.5 tile = 48px, 数学等价) — 若 0.5s 下
     聚合通过, 说明"节奏"是唯一敏感变量, 迁移落地后 MCTS 统一照做;
     若仍负回归, 说明 executor 路径还有未识别的行为差 (如 combo.hit
     双写/VFX rng 抽号), 继续二分
   - C. 放弃迁移, MCTS 感知单独打补丁 (build_sim_state 空手时读
     legacy 0.5s 常量) — 双轨永续, 技术债保留
3. 迁移落地后收尾照旧: MCTS build_sim_state 感知统一 + 删
   Player::can_attack 死代码

## ⭕ 第 2 次会话关键增量 (2026-09-08 下午, 判决性证据)

### A. 单局批次对照 — 迁移本身无害, 是"串行批内局间泄漏"放大了它

`--sim 1` (每批只有 run0, 无跨局传播) × 5 种子, 基线 vs v2 迁移版:

| seed | 基线单局 | v2 单局 |
|---|---|---|
| 101 | F10 / 2360 dmg | F5 / 761 |
| 102 | F6 / 1149 | **F10 / 3859** |
| 103 | F8 / 1947 | F6 / 1568 |
| 104 | F6 / 1150 | **F12 / 5931** |
| 105 | F5 / 272 | F3 / 399 |

**无 F1 全灭形态** (最差 F3), af 均值 7.2 vs 7.0 — 单局层面持平偏好,
两局大幅更好。**迁移的 executor 链路完全健康** (探针: run0 fist_calls=306
fired=27 hits=27 — 出手节奏 0.35s/击, 命中率 100%)。

### B. 串行批次 — run2 起空手局零出手 (跨局泄漏实锤)

30 局 s3: run0/1 正常 (捡 crossbow/spear 起飞), **run2-29 空手局
`_weapon_attack` 零调用** (per-run 探针, 挂在 `_collect_sim_stats` 输出)。
同 exe 两批 6 局日志逐字节一致 → **确定性批内局间泄漏, 非进程随机**。
6 局批次同样形态: run0-1 af=10/10.5, run2 起崩到 4-5。

### C. 已排除项 (二分/探针累计)

- weapons.json 单独改 (fist 48px/0.35s): **零行为差** (legacy 不消费它)
- `_weapon_attack` 空手跳过 `p.combo.hit`: 零行为差 (D2 combo 在 executor
  路径只影响镜像观察/技能 heavy, 不影响伤害 — executor 用 stage mult)
- executor 命中判定/伤害计算/统计口径: §二遗留项全部复核无恙
- 旧 C4PROBE 盲区已补: 原探针只采样圈内 (d≤48px), 基线 72% 时间最近怪
  在 144px 外 (d7) — 圈外站桩/游走是基线空手 AI 的**常态**, 非异常

### D. 泄漏嫌疑清单 (下次会话第一入口, 按优先级)

跨局存活且 new_game 清不到的状态:
1. **EventBus 单例 listener** (SkillEvolution/RuleChain/AttackEvo 注册于
   `_ready`, 100 局共享一个 GameScene — listener 内部静态计数跨局累计?)
2. **`SkillEvolutionManager` uses 计数** (日志见 uses=20 进化 — 跨局残留
   会让 run2 起技能直接处于进化态? 需查 new_game 是否清)
3. `CombatFeelSystem`/`_presentation` 静态? (last_combo_announced 等)
4. `SaveManager`/`g_meta` readonly 模式下的读路径副作用
5. DungeonGenerator `_local_rng` (若 seed 未每局重置 — 但 seed_rng 每局跑)

**定位法建议**: run2 开局首帧打 "agent 首次决策快照" (best_action +
各 evaluator 分值), 对比 run0 开局同点 — 分岔点应该在**第一个决策帧**就有
差异, 顺着差异的 evaluator 定位泄漏的状态变量。

### E. 会话 2 已完成步骤 (可跳过)

1. weapons.json fist 48px/0.35s — 改过, 回退 (验证零行为差)
2. player_controller legacy 删除迁移 — 改过, 回退 (单局批次验证通过)
3. sim_ai `_evaluate_attack` 圈外探针 (d4-d7 采样) — 改过, 回退
   (**建议下次保留此探针** — 它补了 C4PROBE 最大盲区)
4. game_scene `_collect_sim_stats` per-run 探针 — 改过, 回退 (模板见
   git stash 历史/本文件描述: 全局计数器 + run 结束打印清零)

## 一、双轨现状 (问题定义, 已验证)

| 路径 | 判定 | 冷却 | 命中几何 | 消费方 |
|---|---|---|---|---|
| 持械 | `WeaponComponent::can_attack` (can_act: recovery/fatigue/special + 0.15s CD) | 段恢复 0.16~0.45s | weapons.json HitShape | WeaponExecutor |
| 空手 | `Player::can_attack` (`_last_attack_time`+0.5s 常量) | 0.5s 固定 | `find_attack_target` 48px 圆 | GameScene legacy 路径 |

危害: MCTS/AI 感知源错轨 (P1-C6 已证), 空手/持械两套命中语义, 教程场景
(G10.8-B1) 已全走 executor — 正式游戏 legacy 是最后残留。

## 二、本批已完成 (探针结论, 全部已验证)

1. **影响面拓荒**: `can_attack` 全调用点 = 空手 legacy (player_controller:516,
   已在本批迁移删除) / WeaponExecutor:194 / tutorial:312 / MCTS build_sim_state
   (sim_ai.cpp:44) / Monster/Boss 侧 (不相关)
2. **迁移实现过一版并冒烟**:
   - weapons.json fist_basic stage: range 1.0→1.5, recovery 0.15→0.35
   - player_controller: 删 legacy 分支 (~70 行), 空手走 `_weapon_attack`
   - sim_ai build_sim_state: 改读 `weapon.can_attack` (MCTS 感知统一)
   - 结果: **run0/1 起飞** (F2/F9 卡墙→F10/F11, dmg 311/1469→4997/4561,
     持械局因 AI 感知修复大幅变好) 但 **run2-19 空手 F1 全灭 dmg=0**
     → 负回归, 已回退
3. **链路探针已锁定的事实** (probe 输出在会话记录):
   - executor FIST 链完好: stage=0 恒命中, CIRCLE 48px 判定正常
   - 出手节奏 ~0.35s/击 (recovery 拦截正常, 7s/21调用 中放行 1 次 —
     combo_index≠0 时 CD=0, 只剩 recovery 门)
   - **空手局玩家站桩**: 21 连续出手帧怪距恒 40px (双方站桩对峙)

## 三、遗留谜团 (下场第一入口)

**run2-19 空手 F1 dmg=0 的根因未定位**。已排除:
- executor 命中判定 (CIRCLE 48px, origin 同 rect 中心 — 与 legacy 数学等价)
- 伤害计算 (`get_effective_attack`, max(1,dmg) 保底)
- 统计口径 (`_sim_dmg_dealt` 是怪 HP-diff 兜底法, 与路径无关)
- stage 越界 (`current_stage()` 有 min(combo_index, stage_count-1) 钳制)

**未排除的怀疑点 (按优先级)**:
1. **空手局站桩死循环的新节奏**: 探针显示玩家恒选 attack (300/300 帧)、
   怪恒 40px。executor 0.35s/击 vs legacy 0.5s/击 — 同量级但**决策分布
   分岔从 run2 起** (run0/1 大幅变好后的 rng 混沌传播)。需要区分
   "行为真变差" vs "混沌分岔的方差" — **同 run 对比无效 (P1-C6 §四.3),
   应跑 5 种子×100 局看聚合** (下场第一件事)
2. `_evaluate_move` FIST 步进 0.4 分与 attack 基线分 0.67 的老问题
   (P1-A4: 60 拦截:3 命中) — 迁移后出手更快应**缓解**站桩, 为何 run2-19
   更差? 看 C4PROBE: d2=76% (vs 基线 22%) — 决策分布大变, 但含义未解
3. combo_index 语义: fist 单段武器 advance 后 combo_index 循环 1→2→0,
   `can_attack` 的 `combo_index==0 ? 0.15 : 0.0` 对 fist 恒 0 CD —
   设计上 fist 有意快节奏, 但 recovery 0.35 是新加的硬门, 两者交互
   未按 fist 语义审过

## 四、下场行动清单 (会话 2 修订 — 泄漏优先于迁移)

> **顺序调整**: 单局批次已证迁移无害 (§A), 串行批次已证泄漏存在 (§B)。
> 泄漏不修, 一切串行批数据不可判读 — **先修泄漏 (P1-C7-B), 再落地迁移**。

1. **[P1-C7-B 立项] 定位批内局间泄漏** (§D 嫌疑清单 ×5 + §D 定位法):
   run2 开局首帧 agent 决策快照 vs run0 同点, 顺 evaluator 分差异追状态变量。
   **泄漏是基线与 v2 共有的** (基线 run2+ 也受影响, 只是形态池不同) —
   修好它对 500 局基线数据可信度也是地基级修复 (P1-C4 UAF 同级别)
2. 泄漏修复后重做迁移 (§二的 diff + §E 保留项), 验证跑串行 6 局 ×2 批 +
   单局 ×5 种子双维度 (任何一维负回归都拦下)
3. `python tools/p1c5_weapon_ranges.py` 确认 fist 参数现状
   (已回退, 应为 range=1/recovery=0.15 原始值)
4. 迁移落地后顺手统一 MCTS 感知 (build_sim_state 读 weapon.can_attack,
   已验证一行版) + 删 `Player::can_attack`/`ATTACK_COOLDOWN` 死代码

## 五、相关文件

- 本批数据: reports/p1c7a/p1c7a_v3_s{3,7,11,19,23}.out (5 种子×100 聚合, 负回归证据)
- 本批探针输出: reports/p1c6/p1c7a_v3_*_s3.out (C7AV3 链路/C7ADEC 决策/C7AGATE2 门)
- 本批 review: docs/P1C6_COOLDOWN_AWARENESS_NEGATIVE_RESULTS_REVIEW.md (§四.1 提出双轨问题)
- 基线数据: reports/p1c4/p1c4_s{3,7,11,19,23}.json (5 种子×100 局)
- 汇总工具: tools/p1c7a_summary.py (通用 5 种子聚合器, 本批新增)
- 教程先例: tutorial_scene.cpp:312 (G10.8-B1 空手已走 executor, 实机验证过)
