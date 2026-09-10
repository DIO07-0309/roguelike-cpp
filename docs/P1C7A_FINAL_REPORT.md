# P1-C7-A · 双轨判定统一 — 结项报告 (FINAL)

> 日期: 2026-09-10 (第 5 会话: 四任务一天完成, 全部落地) · 状态: **已结项**
> 提交链: 42f3859 (T1 迁移) → 0b0a807 (T2 收尾) → 8152901 (T3 reset 拆分) → 本次 (T4 + 结项)
> 最终形态: 空手/持械统一 WeaponExecutor 轨, 节奏保持 0.5s, MCTS 感知同源,
> Player::can_attack 死代码已删, Boss reset 语义拆分补全

---

## 一、结项判定 (全部通过)

| 任务 | 判据 | 结果 |
|---|---|---|
| T1 B 方案迁移 | 冒烟 20 局 ±5% + 聚合 5 种子×100 | af 6.30/6.25 (+0.8%), 聚合 **af 1.66 vs 1.60 (+3.8%), dmg 200.8 vs 196 (+2.4%)** ✓ |
| T2 MCTS 统一+死代码 | 6 连跑确定性 + 60/60 测试 + 零行为差 | 全过 ✓ |
| T3 Boss reset 拆分 | 零行为差 (逐字节) + 60/60 测试 | 除编译时间戳外逐字节一致 ✓ |
| T4 spear special 探针 | 30 局残留检查 | **0 残留** — 嫌疑划掉 ✓ |

## 二、B 方案为什么成了 (v1/v2/v3 为什么败了)

- **B 方案 diff**: weapons.json fist `recovery 0.15→0.5` (与 legacy
  ATTACK_COOLDOWN 等值) + `range 1→1.5` (48px 等值) + player_controller
  删 legacy 分支 (-70 行)
- v2/v3 败因: recovery 0.35 (比 legacy 快 30%) 改变 AI 决策相位 —
  杀怪更快→目标切换更早→出圈窗口更长→搜刮分支 (0.6) 赢得决策→
  战斗-资源震荡→被未死怪追杀毒磨死。Q3.15 决策权重表按 0.5s 标定
- B 方案验证了机制假设: **节奏是唯一敏感变量, 判定几何/伤害公式/
  多目标命中的差异全部无害** (聚合还小幅偏好: fist CIRCLE 判定圈内
  群杀 + executor 暴击梯度, s3 af 2.05→2.89)
- 三次失败换来的方法论: 出手时序、判定圈、决策权重是耦合系统,
  迁移动"节奏"必翻车, 动"实现"可以

## 三、T2 过程中的插曲: 分岔假阳性 (P1-C4 手册再验证)

T2 改动 (删 can_attack 死代码 + MCTS 感知一行) 后冒烟 20 局出现双跑分岔,
一度怀疑 UB。二分排查 (bisect A/B/A2) + N 次重复:

- T1 exe 4 连跑: 1 哈希 (确定)
- bisect B (只删死代码) 4 连跑: 1 哈希 (确定)
- 完整 T2 6 连跑: **1 哈希 (确定)** — 最初的双跑分岔是 2 次采样的假阳性
- 教训重申: **间歇性判定 N≥4** (P1-C4 手册原话, 本次差点重蹈覆辙)

## 四、T3 的事实修正 (WIP 前提有误)

WIP 称 "BossSystemDirector::reset() 全仓零调用" — **不成立**。
`init_events()` (game_scene _ready 调用) 注册了 FLOOR_ENTER EventBus
订阅, lambda 里调 reset() — 换层时一直在跑。直接调用 grep 漏掉了
事件路径。实际修复内容:

- reset() → `reset_floor()` (语义命名清晰化) + `reset_run()` 别名
  (boss 子系统审计后确认无"局内跨层须持久"字段 — replay_mem 由
  init_on_spawn 每战重建, mirror 跨局记忆走 export/inject 独立通道)
- enter_floor 手动调用点 (与 EventBus emit 重复但幂等, 语义显式化)
- new_game 补 reset_run (原跨局无 reset — 但 new_game 后必进
  enter_floor(1) 的 emit, 故零行为差实证成立)

## 五、遗留

- 无 P1-C7 遗留。镜像冻结/搜刮预算等此前修复均已包含
- 下一个大方向候选 (与 C7 无关): F1 死亡率 88%→? 的 AI 行为深水区
  (P1-C1 文档判定"数值带已到顶, 剩余交给 AI 行为")

## 六、验证清单 (本次会话累计)

| 验证 | 结果 |
|---|---|
| 基线复现 (T1 前) | af=6.25/dmg=1746.2 精确 ✓ |
| T1 冒烟 s3 20 局 | af 6.30 (+0.8%) ✓ |
| T1 聚合 5×100 | af 1.66/dmg 200.8 ✓ |
| T2 确定性 6 连跑 | 1 哈希 ✓ |
| T2/T3 全测试 | 60/60 ×2 ✓ |
| T3 零行为差 | 逐字节 (除时间戳) ✓ |
| T4 special 残留 | 0/30 局 ✓ |

## 七、相关文件

- 迁移数据: reports/p1c7a/p1c7a_b1_s{3,7,11,19,23}.out (聚合通过证据)
- 排查数据: reports/p1c6/p1c7a_t2_* (bisect/6x), p1c7a_t3_verify20_s3.out
- T4 证据: reports/p1c6/p1c7a_t4_spec30_s3.out (30 局 zero-residue)
- 前史: docs/P1C7A_DUAL_TRACK_UNIFICATION_WIP.md (三次失败全记录, 保留)
