# P1-C7-A · 双轨判定统一 — 进行中交接 (Work In Progress)

> 日期: 2026-09-09 (第 3 次会话: P1-C7-B 泄漏排查完结) · 状态: **实验中途, 已回退到干净基线**
> (e542af0 行为等价: af=6.25/dmg=1746.2 精确复现)
> 方案: 空手 (fist_basic) 从 legacy `Player::can_attack` (0.5s) 轨迁移到
> WeaponExecutor/WeaponComponent 轨 (数据驱动), fist stage 调至 range=1.5
> (48px 手感保持) + recovery=0.35 (≈0.5s 出手间隔)。

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

### P1-C7-A 下一步 (第 4 会话入口)

1. **放弃"泄漏修复后迁移自然通过"的路线** — 泄漏不存在。
   迁移要落地必须解决"战斗-搜刮再平衡": 候选 = 搜刮分支加"近距有
   未死怪时压制" (15 血怪在 3 格内时 approach 压过搜刮 0.6), 或接受
   v2 形态跑 5 种子×100 聚合看总体 (单 s3 串行批不可判读 — 已两次证明)
2. 迁移 + 再平衡一起冒烟 (20 局 s3) + 5 种子×100 聚合双验证
3. 落地后: MCTS build_sim_state 感知统一 (一行版已验证) + 删
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

- 本批 review: docs/P1C6_COOLDOWN_AWARENESS_NEGATIVE_RESULTS_REVIEW.md (§四.1 提出双轨问题)
- 基线数据: reports/p1c4/p1c4_s{3,7,11,19,23}.json (5 种子×100 局)
- 教程先例: tutorial_scene.cpp:312 (G10.8-B1 空手已走 executor, 实机验证过)
- 探针输出样例: [P1C7PROBE] (会话记录 2026-09-08 ×2 会话, .out 在 reports/p1c6/ 未入库)
