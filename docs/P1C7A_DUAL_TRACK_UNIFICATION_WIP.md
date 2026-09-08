# P1-C7-A · 双轨判定统一 — 进行中交接 (Work In Progress)

> 日期: 2026-09-08 · 状态: **实验中途, 已回退到干净基线** (e542af0 行为等价)
> 方案: 空手 (fist_basic) 从 legacy `Player::can_attack` (0.5s) 轨迁移到
> WeaponExecutor/WeaponComponent 轨 (数据驱动), fist stage 调至 range=1.5
> (48px 手感保持) + recovery=0.35 (≈0.5s 出手间隔)。

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

## 四、下场行动清单

1. `conda run python tools/p1c5_weapon_ranges.py` 确认 fist 参数现状
   (本批已回退, 应为 range=1/recovery=0.15 原始值)
2. 重做迁移 (本文件 §二的实现 diff 在 git 历史/会话记录可抄), 但
   **验证改为 5 种子×100 局聚合** (s3/s7/s11/s19/s23, 基线数据
   reports/p1c4/*.json 可直接对比), 不再用 20 局单种子判定
3. 若聚合仍 F1 空手全灭: 上 per-run fist 命中/出手探针 (本批 probe 模板
   在 player_controller.cpp 会话记录, 24 次采样式), 区分"站桩"vs"出手未中"
4. 备选: 若聚合显示 F1 只是方差、整体 af/dmg 持平或更好 → 直接落地,
   F1 空手死亡属已接受基线病理 (91% F1 死亡率本就是 P1 系列主攻目标)
5. 迁移落地后顺手统一 MCTS 感知 (build_sim_state 读 weapon.can_attack,
   本批已写好一行版) + 删 `Player::can_attack`/`ATTACK_COOLDOWN` 死代码

## 五、相关文件

- 本批 review: docs/P1C6_COOLDOWN_AWARENESS_NEGATIVE_RESULTS_REVIEW.md (§四.1 提出双轨问题)
- 基线数据: reports/p1c4/p1c4_s{3,7,11,19,23}.json (5 种子×100 局)
- 教程先例: tutorial_scene.cpp:312 (G10.8-B1 空手已走 executor, 实机验证过)
- 探针输出样例: [P1C7PROBE] fist#1..24 (会话记录 2026-09-08)
