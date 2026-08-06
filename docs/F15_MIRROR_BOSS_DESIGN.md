# F15 Mirror Boss 设计文档 — Player Clone Agent

> 状态: DRAFT (M0 核对完成, 待 M1 开工)
> 日期: 2026-08-06
> 范围: F15 终章 Boss "Ending Echo" — 学习玩家整局操作并镜像反制

---

## 1. 设计意图（叙事 → 机制）

Ending Echo 的主题是 **"复制当前这一次冒险中的玩家人格"**：
Boss 拥有玩家的武器、技能、属性，并且能预测玩家的下一步行为，针对玩家习惯进行反制。

三阶段觉醒由 **MirrorAgent 学习进度 + 战斗状态** 共同驱动，血量/时间/濒死只作辅助条件：

| Phase | 名称 | 行为 |
|---|---|---|
| 1 | 观察 (Observe) | 收集当前战斗数据, 比对与 F1-F14 画像一致性; Boss 模仿玩家基础攻击/技能 |
| 2 | 镜像 (Mirror) | 用预测结果反制: 提前打断技能、针对弱点调整距离、复用玩家连招 |
| 3 | 进化 (Evolve) | 压制模式: 完整技能组合、改变策略、形成"超越玩家的自己" |

## 2. M0 现状核对（代码勘察结论）

### 2.1 已实现（可复用, 无需重写）

| 组件 | 文件 | 覆盖用户需求 |
|---|---|---|
| PlayerAction 事件流 | `src/game/ai/player_behavior/player_action.h` | ATTACK/SKILL/MOVE/DODGE/TAKE_DAMAGE/DEAL_DAMAGE/HEAL/FLOOR_ENTER |
| F1-F14 采集器 | `player_behavior_recorder.h/.cpp` | 武器/技能/移动/闪避/受伤/治疗/楼层 全挂载点; behavior.json 导出 |
| 行为数据聚合 | `player_behavior_data.h` | 武器计数/技能计数/受伤分布/移动/闪避/楼层 |
| 画像分析器 | `player_behavior_analyzer.h/.cpp` | PlayerAction 流 → PlayerHabitProfile |
| 画像结构 | `player_habit_profile.h` | 战斗风格/技能偏好/连招/低血行为/闪避习惯/反制提示 |
| MirrorAgent | `src/ai/mirror/mirror_agent.h/.cpp` | 三阶段状态机 + 距离建议 + 打断/压近 + 预测 + M4e 在线 Thompson |
| 在线自适应策略 | `src/ai/mirror/online_adaptive_policy.h` | Beta 后验 Thompson 决策; export/import_memory = 跨局 Meta Memory 接口 |
| F15 战斗控制器 | `src/game/director/mirror_combat_director.h/.cpp` | 武器镜像 3 段 combo + 技能镜像 + 行为状态机, 组合模式 |
| RL 联动 | `mirror_agent.cpp` F15.4 | mirror_reward + style_to_int → SimulationState |

### 2.2 差距（本设计要新增的部分）

| 编号 | 差距 | 用户需求原文 |
|---|---|---|
| G1 | **行为克隆 state→action 映射表缺失** — 现有 `predict_next_action` 是手写规则, 未从 PlayerAction 流学习决策 | "使用行为克隆方式建立 state→action 映射, 学习玩家在不同战斗状态下的决策, 而不是只统计次数" |
| G2 | **Phase 2/3 触发是纯计时** (`tick_phase_timer` 30s/60s) | "Phase 2 触发: 预测准确率达阈值 / 已观察关键行为数量 / 战斗时间达最低学习窗口; 血量时间濒死仅辅助" |
| G3 | **Phase 1 无"玩家当前行为 vs F1-F14 画像一致性"比对** | "分析玩家当前行为是否与 F1-F14 画像一致" |
| G4 | **预测准确率在线滚动评估缺失** | 准确率 = Phase 2 触发依据之一 |
| G5 | 可选小型 ML 模型插槽未预留 | "可选的小型ML模型" (仅接口, 不强制) |

## 3. 目标架构

```
┌─────────────────────────────────────────────────────────┐
│  F1-F14 PlayerAction Stream (已有)                       │
└──────────────┬──────────────────────────────────────────┘
               ▼
┌─────────────────────────────────────────────────────────┐
│  PlayerBehaviorAnalyzer (已有) → PlayerHabitProfile      │
└───────┬───────────────────────────────┬─────────────────┘
        ▼                               ▼
┌───────────────┐          ┌────────────────────────────┐
│ 规则统计层     │          │ 行为克隆层 (G1, 新增)        │
│ profile hints │          │ BehaviorCloneTable:         │
│ (已有)         │          │ obs_key → 动作分布           │
└───────┬───────┘          │ 稀疏 → 画像级降级             │
        └───────────┬──────┴────────────────────────────┘
                    ▼
┌─────────────────────────────────────────────────────────┐
│  MirrorAgent 决策仲裁 (已有 + 扩)                        │
│  规则层 / 克隆层 / Thompson 在线层 / (G5) 可选小 ML 插槽   │
└───────────────┬─────────────────────────────────────────┘
                ▼
┌─────────────────────────────────────────────────────────┐
│  MirrorCombatDirector (已有) — 武器/技能镜像执行          │
│  三阶段觉醒: Phase 1 观察 / 2 镜像 / 3 进化 (G2 改动态)    │
└─────────────────────────────────────────────────────────┘
```

## 4. 新增模块设计

### 4.1 BehaviorCloneTable（G1, M1 范围）

- 位置: `src/ai/mirror/behavior_clone_table.h/.cpp`
- 构建: 遍历 F1-F14 PlayerAction 流, 对每个决策点（ATTACK/SKILL/MOVE/DODGE/HEAL）记录
  当时上下文 `(dist_to_enemy_bucket, player_hp_bucket, 技能冷却位)` → 动作分布
- 上下文桶: 复用 `OnlineAdaptivePolicy::bucket_for` 的风格 (距离×HP 2D 桶), 避免新哈希
- 查询: `predict(state) → 最高概率动作 + 置信度(概率值)`
- **稀疏降级链**: 精确桶 → 同距离桶合并 → 画像级默认(profile 规则) → 兜底 ATTACK
- 约束: 纯数据转换, 无游戏对象依赖, 单测可覆盖

### 4.2 动态 Phase 触发（G2/G3/G4, M2 范围）

- 新增在线滚动评估: 每帧 BOSS 出招前做 1 次"预测玩家下一动作", 命中/落空入
  环形窗口(最近 32 次), `accuracy = 命中/32`
- Phase 1→2 触发 (任一):
  1. 滚动准确率 ≥ 0.65
  2. 已观察关键行为 ≥ 40 次 (攻击/技能/闪避/治疗各计)
  3. 战斗时间 ≥ 20s (最低学习窗口)
- Phase 2→3 触发:
  1. 识别核心模式 (同桶预测命中 ≥ 10 次 且 准确率 ≥ 0.7)
  2. 玩家 HP < 35% 或 BOSS HP < 35% (辅助条件)
- Phase 1 一致性比对 (G3): 战斗中每 5s 采样一次当前战斗统计
  (攻击频率/技能偏好/闪避率), 与画像比较 → 输出 `profile_drift` 分数,
  供 Director 微调模仿行为 (漂移大 = 玩家已换打法, 模仿降权)

### 4.3 MirrorAgent 决策仲裁扩展（M3 范围）

- 现有: 规则层 (profile hints) + Thompson 在线层
- 新增: 克隆层插入 — Phase≥2 时克隆层置信度 > 0.5 优先克隆层,
  否则 Thompson; 规则层始终保持兜底
- G5 插槽: `set_ml_predictor(std::function<PlayerActionType(state)>)` 预留,
  当前注册 nullptr 即关闭

## 5. Milestone 拆分

| Milestone | 内容 | 验证 |
|---|---|---|
| M1 | BehaviorCloneTable 构建 + 查询 + 稀疏降级 | 单测: 构造 action 流 → 表 → 预测命中 |
| M2 | 动态 Phase 触发 + 滚动准确率 + 一致性比对 | 单测: 模拟命中/落空 → 阈值触发 |
| M3 | MirrorAgent 仲裁接入克隆层 + ML 插槽 | 集成: --sim 冒烟无崩 |
| M3-AC | **后验验收: 证明 AI 链路真闭环** | 下节 §7 五项验收 + MirrorDebugStats |
| M4 | F15 实战调参 (触发阈值/降级权重) | World Validator + 全测试 + 冒烟 |

## 6. 风险与决策

1. **单局样本量 (数百~千条)**: 行为克隆表只覆盖高频状态 — 降级链保证低血/治疗等
   稀有状态回落到画像规则, 不产生空洞决策
2. **准确率阈值抖动**: 滚动窗口 32 次平滑, 单次运气不触发
3. **技能镜像映射**: MirrorSkillDef 需要确认玩家技能→Boss 出招槽映射细节
   (M4 时核对 director init), 位移类技能做适配
4. **跨局 Meta Memory**: `export/import_memory` 已存在, 仅预留,
   不参与本版本核心 AI (按用户决策)

## 7. M3 后验验收 — 证明 AI 链路真闭环 (非"存在未调用")

验收工具: `MirrorDebugStats` (`src/ai/mirror/`) + 战斗中 **F9** 切换 HUD 统计 + 战斗结束 `[MIRROR-ACC]` 日志摘要。

### 7.1 验收目标链路
```
F1-F14 行为采集 → PlayerAction → 分析器 → PlayerHabitProfile
  → BehaviorCloneTable (M1) → MirrorAgent → MirrorCombatDirector → Echo 出招
```

### 7.2 手工验收流程 (1 局 ~10-15 min)
1. **测试A — 回血习惯 (验证 CloneTable 驱动战斗)**: 前 14 层刻意在 HP<30% 时立即回血 (~10+ 次)
2. F15 进入终焉回响战斗, **按 F9** 打开 MIRROR AI 统计
3. 观察 HUD: Predict 计数每 0.5s 增长, Phase 1→2→3 时长变化
4. 观察低血+近距离: Echo 是否**靠近/打断/技能加速** (对应 HEAL 意图→APPROACH/SKILL 臂)
5. 战斗结束后看控制台/`game.log` 的 `[MIRROR-ACC] battle ended — ...` 摘要

### 7.3 通过判据 (对照用户 5 验收点)
| # | 验证点 | 通过证据 |
|---|---|---|
| 1 | CloneTable 真驱动战斗 | HUD `CloneHit>0` 且 Phase≥2 时行为臂来自克隆意图 (HEAL→压近) |
| 2 | 调用链统计 | `Predict>0` 且增长; `仲裁[Clone/ML/Tho]` 合计>0; `[MIRROR-ACC]` 摘要出现在战斗结束 |
| 3 | Phase 真改变行为 | 行为分布变化: P1 以 普攻(Attack)为主 → P2 技能(Skill)/压近(App) 占比升 → P3 连招/打断 |
| 4 | 技能真实效果 | 已代码核对 ✅: heals 真实 `boss.combat.heal(max/5)`; 时停真实 `slow` x4; 近战/弹幕/AOE 真实伤害 — 无"名字镜像" |
| — | 不加 ML | M3-AC 不含任何神经网络; G5 插槽默认 nullptr |

### 7.4 验收已知缺陷 (不阻塞验收, 记录后续修)
1. `--sim N` 需在标题画面手动按 N 才进入 (G5.6 无自动开始) — 命令行无人值守验证不可用, 故验收需人工实操
2. 修复: 自愈/时停技能此前缺 `report_outcome` 在线反馈 (臂学不到信号) → 已补正反馈
3. 修复: 退出时 `unload_all()` 双调 double-free (0xC0000374) → 已加防重入

### 7.5 验收结论
单测 30/30 绿 (含 `mirror_acc_test` 10 项: 统计逻辑 + MirrorAgent 真实路径接入)。
链路代码路径全部有计数出口; **实测闭环证据需按 §7.2 实战获得** (CLI 无法注入键盘操作)。

---

## 8. M4 实战复盘 — 镜像战术学习 (待评审设计, v0.9.20 后新增)

> 状态: DRAFT — 用户复盘提出, 先讨论方案再编码 (规范第 9 条)

### 8.1 实战反馈原文 (用户 2026-08-06)

1. **镜像没学会玩家的攻击方式** — 期望: 镜像应能复现玩家战术套路, 例如
   - "一开始用时间暂停靠近我, 暂停结束切近战武器攻击"
   - "一开始用远程武器 (连弩/长矛) 消耗, 等我靠近切近战, 或切远程拉扯"
2. **镜像时停期间玩家不能行动而镜像可以** — 当前镜像 time_stop 只给玩家 `slow×4`,
   非真时停语义

### 8.2 现状核对 (代码勘察)

| 能力 | 现状 | 差距 |
|---|---|---|
| 技能选择 | `_skill_idx = (_skill_idx+1) % size` 循环轮转 (mirror_combat_director.cpp:131) | 无"按战术选技能" |
| 武器切换 | `_init_mirror_boss` 固定一种武器 (wtype), 全场不变 | 无切换机制 |
| 战术组合 | 行为臂仅 4 种: 靠近/攻击/技能/后撤 (L119-149) | 无"时停→突进→切近战"序列 |
| 镜像时停 | case 4: `apply_buff(player,"slow",4)` (L260-266) | 非冻结, 玩家仍可缓慢行动 |
| 克隆表 | `BehaviorCloneTable` 学的是 state→action 映射 (每次决策单步) | 无 2-3 步序列记忆 |

### 8.3 设计选项

**A. 战术脚本层 (入门版, 推荐先行)**
- 在 MirrorCombatDirector 增加 `_tactic` 状态 (开局/消耗/近战/拉扯), 由玩家画像驱动:
  - 画像 `combat_style` 为远程偏好 → 开局远程消耗 + 玩家靠近后拉距离
  - 画像技能偏好含时停 (The World) → 镜像时停后突进近战
- 时停技能从"循环轮转"改为"战术触发": 低血/玩家放技能窗口期优先
- 改动面小: 只在 director 层, 不碰克隆表/仲裁

**B. 动作序列学习 (进阶版)**
- 克隆表从单步 state→action 扩展为 2-3 步 state→action 序列 (时停→攻击 等)
- 玩家时停后接什么动作, 镜像对应反制序列
- 改动面大: 克隆表结构/统计/仲裁都要动

**C. 镜像时停语义修正**
- case 4 改为真时停: 冻结玩家输入/移动 4s (玩家可被命中), 或仿 The World 的
  `time_stop_remaining` 机制 (镜像专属)
- 需防滥用: 冷却兜底 + Phase 限制

### 8.4 决策点 (待用户拍板)

| # | 决策 | 选项 | 已定 |
|---|---|---|---|
| 1 | 本轮做哪档? | A 战术脚本 / B 序列学习 / A+B | ✅ A+B 分阶段: 先 A 战术脚本层, 验证 Echo 能执行玩家战术套路; 后续再扩展 B 序列学习 (避免数据稀疏) |
| 2 | 镜像时停语义 | 修正为真冻结 (C) / 保持 slow 但加强到"禁移动" | ✅ C: Mirror 专属真冻结 — 玩家禁移动/攻击, Echo 可行动; Phase 限制 + 冷却限制 |
| 3 | 武器切换 | 镜像是否支持战斗中切武器 | ✅ 支持, 非随机 — 由玩家画像 + 战术状态驱动 |

### 8.5 M4.X 实施计划 (用户已拍板, 2026-08-06)

**优先顺序**: M4.1 Tactical Layer → M4.2 Time Stop → M4.3 Weapon Switching → 后续 Sequence Learning

**M4.1 战术脚本层 (Tactical Layer)**
- 在 `MirrorCombatDirector` 增加战术状态机 (`_tactic`), 由 `PlayerHabitProfile` 驱动:
  - 开局消耗 (open/ranged poke): 画像远程偏好 (连弩/长矛) → 开局远程消耗 - 玩家靠近后拉距
  - 压力突进 (engage): 画像近战/低血压近 → 主动贴近
  - 拉扯 (kite): HP/距离条件 → 边打边拉
  - (M4.2 后接入时停突进)
- 技能选择从"循环轮转"改为"战术映射": 时停技能 (World) 仅在战术需要时出, 不复用轮转
- 单测: 战术状态切换按画像输入可复现

**M4.2 镜像专属时停 (Time Stop)**
- `_mirror_skill` case 4 从 `slow×4` 改为**真冻结**:
  - 玩家输入/移动/攻击禁用一段时间
  - Echo 可自由行动 (镜像专属, 不共享玩家 time_stop_remaining)
- 限制: 仅在 Phase≥2 可用; 冷却兜底 (复用 `_skill_cd_timer`, 不与普通技能共用)
- 需要在 PlayerController 增加"镜像冻结期间禁操作"门控 (仅 active 时)
- 单测: 冻结状态下玩家 attack/move 调用被拒绝

**M4.3 武器切换 (Weapon Switching)**
- `_init_mirror_boss` 从固定单武器改为多武器池 (玩家画像选到的武器集合)
- 战斗中按战术状态切换: 远程消耗用远程武器 → 近战阶段切近战武器
- 由画像 (武器计数) 决定主/副武器

**M4.4 (后续) 序列学习 (Sequence Learning)**
- 克隆表从单步 state→action 扩展 2-3 步序列 (时停→攻击 等), 玩家时停后接什么动作镜像反制
- 数据稀疏规避: 序列表仅在高频路径建条目, 低频回落单步克隆


