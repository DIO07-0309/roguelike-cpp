# 地牢肉鸽 — Roguelike C++

> **v1.0.0 正式版** — C++17 + Raylib 5.0 | CMake | **281 源文件** (127 cpp + 154 h) | Windows 实机验证
>
> 随机生成 15 层地牢，击败 Boss「终焉回响」通关。
> 五项 Stable 冻结验收通过：**API / Save / Mod / Regression / Performance**（报告 `docs/V1_0_0_ACCEPTANCE.md`）

本项目定位：

1. **完整可玩的游戏** — 3 章 15 层、5 场 Boss 战、武器/技能/元素/圣物/局外成长全链路
2. **数据驱动架构** — 20+ JSON 配置 → Registry → 运行时，Mod 可热插拔
3. **游戏 AI 研究平台** — 行为树 / MCTS / Q-Learning / 镜像学习 Boss，`--sim` 批量评估

---

## 快速开始

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/roguelike_cpp.exe
```

桌面版直接双击 `Roguelike-CPP版/roguelike_cpp.exe`。

---

## 操作

### 移动与战斗

| 按键 | 功能 |
|------|------|
| **WASD** | 移动 |
| **↑↓←→** | 切换朝向 |
| **空格** | 普攻（武器三连击） |
| **1~4** | 主动技能 |
| **E** | 拾取 / 触发特殊房间 |
| **I** | 背包（X装备 U使用 D丢弃） |
| **R** | 圣物面板 |
| **>** | 下楼（清空楼层后） |

### 系统

| 按键 | 功能 |
|------|------|
| **N** | 新游戏 |
| **C** | 继续 |
| **F** | 选关 |
| **T** | 教程 |
| **Esc** | 返回标题 |
| **F11** | 全屏 |

---

## Feature 总览

### 核心循环（实时动作）

即时制战斗：攻击间隔 0.5s，HitStop 打击感 + 震屏 + 冻结帧 + 连击评分；楼内清怪 → 下楼 → 强化 → Boss。死亡即重开（肉鸽），局外 MetaProgression 10 节点永久成长。

### 地牢生成

BSP 二分划分随机地图，3 章 × 5 层（F1-5 地牢入口 / F6-10 幽暗深渊 / F11-15 虚空深渊），F4/9/14 休整层、F5/10/15 Boss 层。10 类特殊房间（祭坛/宝箱/泉水/商店/铁匠/图书馆/赌徒/神殿/隐藏密室/地标）+ ArenaObject 战场元素（毒池/尖刺/图腾/**木桶 — 攻击或弹体点燃 → 0.6s 引信 → AOE 爆炸**）。F10 地狱火魔 Boss 房含机制地形（LAVA 灼烧 + 中央安全区熔岩环带）。

### 战斗系统

- **武器 (G9)** — 6 种 × 3 段连击 × 5 命中判定（拳头/匕首/长剑/双截棍/连弩/长矛），21 条 JSON 配置，传奇特效 + 攻击标签联动
- **技能** — 22 条：16 主动（4 基础：斩击/神罚/自愈/The World + 12 变体）+ 6 被动，每技能 3 级进化（使用次数驱动）
- **元素核心 (G10)** — 开局永久三选一：火焰暴击 / 冰霜冻结 / 剧毒 DOT，独立成长 + VFX
- **Boss** — 5 场（F5 三选一）：暗影骑士 / 亡灵法师 / 血族伯爵 / **地狱火魔(F10)** — 弹幕图案化（扇形/环形/螺旋多波）+ 机制阶段（核心破坏→弹幕风暴→易伤）+ 领域地形 / **终焉回响(F15)** — 镜像学习 Boss
- **敌人** — 30 种，9 类 AI（含 SNIPER/CONTROLLER/AMBUSH/GUARDIAN 原型），全数据驱动

### 内容规模

| 类别 | 数量 | 类别 | 数量 |
|------|------|------|------|
| 敌人 | 30 | 圣物 | 60 |
| Boss | 5 | 物品 | 31 |
| 技能 | 22 (16 主动 + 6 被动) | 任务 | 12 |
| 武器 | 21 | 结局 | 5 |
| 对话 | 34 (Boss 自适应) | 遭遇 | 9 |
| 配置 JSON | 20+ | 中文字体码点 | 1769 (1663 CJK) |

### 系统能力

- **存档 v3** — 跨版本兼容（v1→v3 + 增量字段），SaveStable 3 验收测试
- **Mod 系统** — `mods/` 扫描 + 依赖解析 + MergeMode{Skip/Replace/MergePatch} 字段级合并
- **中文 UI** — 生成字体图集全中文渲染，`tools/extract_chars.py` 自动维护码点
- **音频** — 程序化合成（wave_synth）：8 SFX + 4 BGM + 交叉淡入 + Boss Phase2 cue
- **回放/确定性** — Replay 录制 + hash 链逐帧校验（`--record/--replay`）
- **批量评估** — `--sim N` headless 模拟 + 平衡报告（`reports/balance_report.json`）

---

## AI 架构

### 玩家侧决策（模拟器驱动）

| 系统 | 说明 |
|------|------|
| **DecisionAgent** | 评分式决策（`src/core/sim/sim_ai.cpp`）：attack/skill/move/pickup/heal 计分取最大，BuildType 12 流派感知 |
| **BTAgent** | 行为树（`src/ai/agents/bt_agent.cpp`）：根 Selector 8 子节点优先序 — BossIntro→确认 / Stairs→下楼 / 低血→自愈 / BossNear→攻击 / AoE / EnemyNear / 拾取 / Wander 兜底 |
| **MCTS** | `--sim-ai mcts`：UCT 搜索 C=1.414，100 次迭代，深度上限 10，回溯折扣 0.95，终局 ±1000 |
| **Q-Learning** | `--sim-ai decision` 内的 RL 环境（G8.4）：7 维观测 → Q 表离散化 |

### 仲裁链（镜像 Boss 决策，五层）

`src/ai/mirror/mirror_agent.cpp`：

```
ML 插槽 → 战术链(n-gram) → RL(Q 表 exploit) → 克隆(行为预测) → Thompson 采样
```

实测仲裁分布（v0.9.30，500 局）：`[Clone:0 ML:0 RL:11/25/26 Tho:0]` — RL 完全接管。

### RL 训练管线

- `--rl-train N`：通用 Q 表（`saves/rl_qtable.json`，~2380 条目）
- `--rl-mirror N`：镜像 Boss 自博弈，4 风格 Q 表（`saves/rl_mirror_q_<STYLE>.json`）
- epsilon 退火 0.12→0.005，末段 10% 低探索统计收敛：AGGRESSIVE 96.8% / DEFENSIVE 99.0% / SNIPER 96.4% / BALANCED 99.2% / TRAIN 100%
- 运行时按玩家画像风格加载（缺失安全降级跳过）

### 导航与模拟

- 模拟器用 **BFS**（`_bfs_toward/_bfs_away`，tile 级 rect 碰撞 + 危险避让：熔岩/毒池/尖刺/木桶）
- **A\*** pathfinder（priority_queue + Manhattan 启发式）供 BT MoveToTarget 节点
- `--sim N --sim-seed S`：headless 定步长 1/60，每局统计 → 胜率 / 平均楼层 / Boss 击杀率 / Build 评级（>70% [OP] <25% [UP]）/ 圣物 TOP10 / 威胁度

---
## F15 Mirror Boss — 终焉回响

> 它观察你、学习你，然后用你的方式击败你。

### 镜像机制

| 项 | 实现 |
|------|------|
| **武器同步** | 复制玩家武器 3 段连击（近战=玩家武器 / 远程=CROSSBOW×0.8） |
| **技能同步** | 按玩家主动技能逐槽镜像，关键词→类型映射（自愈/时停/近战/AOE/弹幕/爆发/吸血/召唤） |
| **属性同步** | HP=玩家×2.5，ATK=玩家×0.85，防御=玩家+5/+3，自愈≈6.7% maxHP |
| **真冻结** | Phase≥2 冻结玩家 1.5s（禁移动/攻击），镜像仍可行动 |
| **节奏** | 决策间隔 0.5s，行为状态机 approach/attack/skill/retreat，追击 120px/s |

### 三阶段人格演化

| 阶段 | 机制 | 玩家感受 |
|------|------|----------|
| **Phase 1 观察** | MirrorAgent 实时采集玩家动作（攻击/技能 0.5s 窗口、位移、喝药识别） | Boss 复制你的武器/技能 — *我在打自己* |
| **Phase 2 镜像** | 历史反制策略激活（BehaviorCloneTable 意图预测 + 战术链 n-gram 序列记忆） | Boss 开始预判你的技能与走位 — *它在预测我* |
| **Phase 3 进化** | 在线学习：reward = 命中奖励，Thompson 多臂采样持续探索 | *它比我更懂我* |

### 存档记忆（跨局学习）

- 镜像先验 144 float（桶-major）写入存档 `mra:`/`mrb:` 字段（M4e）
- 新局开始自动注入 → 旧后验叠加为新先验（`inject_mirror_memory`）
- 克隆门槛自适应：策略漂移 >0.5 时置信门槛 0.50→0.75

### RL 自博弈（F15.4）

`--rl-mirror N`：镜像 × 4 玩家风格离线训练 Q 表（95%+ 收敛），运行时按 `profile.style_name()` 加载，观察期（phase<2）不启用。实测 RL 完全接管仲裁。

---

## v1.0 验证数据

### 平衡回归（500 局 / 5 seeds，目标区间 6-10%）

| 版本 | 胜率 | 备注 |
|------|------|------|
| Q3.12 基线 | **5.8%** (29/500) | 死亡分布 F1-5≈40% F6-10≈50% F11-15≈8% |
| v0.9.30 | **6.6%** | RL 决策层接入镜像（Boss 变强，8.6%→6.6%） |
| v0.9.31 | **7.0%** | M4b 地狱火魔领域作战 |
| v0.9.32 | **8.0%** | 五项 Stable 验收 |
| v0.9.33 | **10.0%** | 收官体检（Boss 冷却恢复判定后区间上沿） |

### 确定性

- **Q3.14 对拍**：3 种子 × 20 局 × 2 批 **逐字节一致**（130 万行级）
- 修复三类跨进程分叉：怪物脱卡状态/SimAI 记忆/双节棍追踪 指针键 → instance_id/uint64
- Replay hash 链（`compute_state_hash` 逐帧链式 + `verify_hash_chain`）

### 性能

- sim 500 局（5 进程并行）：**53s**，单核 ~9.4 局/s（支撑万局规模评估）
- 全量测试套件：0.46s

### 测试与验证

- **34 个 ctest 条目全绿**（含 SaveStable 3 验收测试：v1 旧档兼容 / 坏条目容错 / 全字段 roundtrip）
- World Validator：20+ JSON 交叉引用 **0 errors 0 warnings**
- 木桶闭环实测：200 局 sim **38 次 点燃→爆炸 完全成对**（伤害随楼层缩放）
- 收官体检：编译 0 警告（bgm narrowing 修复后）

---

## Current Limitations

诚实清单（v1.0 已知边界）：

- **美术** — 程序化像素 + Kenney CC0 占位素材，无完整商业美术资产
- **手感** — 实时动作（攻击间隔 0.5s），无翻滚/无锁定，非回合制；打击感三件套已就位但数值打磨以研究平衡为主
- **平衡** — 胜率目标区间 6-10%（研究平台定位，非商业难度曲线）
- **Boss 决策链** — D5 决策系统结果目前仅 HUD 展示，实际行为由连招模板驱动（`boss.cpp`）；`boss_decision_to_command` 为显示层
- **导航** — 运行时模拟用 BFS；A* pathfinder 有测试支撑但未接入生产路径
- **环境物** — 原 hazards.json（环境危险物）为死链路已移除（v0.9.33）；当前环境机制为 ArenaObject（毒池/尖刺/图腾/木桶）+ 熔岩地砖
- **中文渲染** — 依赖生成字体图集（1769 码点），新增中文文案需重跑 `extract_chars.py`
- **平台** — Windows 实机验证；macOS/Linux 构建规范见 `docs/G4_PLATFORM_BIBLE.md`，未实机验证
- **输入** — 键盘 only，无手柄/触摸
- **RL 资产** — Q 表生成于本地 `saves/`（训练产物，非仓库内置）；首次运行无 Q 表时镜像自动降级
- **性能** — 单线程模拟（9.4 局/s/核），万局评估需多进程并行（已验证）

---

## 项目结构

```
src/
├── core/          # 引擎框架 + sim 模拟器 (sim_ai/sim_runner) + replay + Mod
├── game/          # 实体/战斗/世界/场景/director/音频/存档
├── ai/            # 行为树/导航(A*)/MCTS/RL/MirrorAgent
├── data/          # JSON 加载器 (20+ 配置)
├── main.cpp       # 入口 + CLI (--sim/--rl-train/--record/--mods)
tests/             # GoogleTest 34 条目
resources/         # 20+ JSON 配置（数据骨干）
tools/             # world_validator.py / extract_chars.py
docs/              # 设计文档（ARCHITECTURE / G4_PLATFORM / WORLD_LORE / D1_GAMEPLAY …）
vendor/            # raylib 5.0 + nlohmann/json
```

---

## 构建与开发

### 编译

```bash
# Release
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build

# Debug + 测试
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build build && cd build && ctest
```

### 命令行参数

| 参数 | 说明 |
|------|------|
| `--sim N` | 跑 N 局模拟（`--sim-seed S` 种子） |
| `--sim-ai bt/mcts/decision` | 模拟 AI 类型 |
| `--rl-train N` | Q-Learning 训练 |
| `--rl-mirror N` | 镜像 Boss 自博弈训练 |
| `--record <path>` / `--replay <path>` | 回放录制/播放 |
| `--sim-build` / `--sim-all-builds` | Build 流派评估 |

### Python 工具

```bash
python tools/world_validator.py    # 20+ JSON 交叉校验（修改配置后必跑）
python tools/extract_chars.py      # 提取 CJK 字符 → 字体码点表
```

### 设计文档

| 文档 | 内容 |
|------|------|
| `docs/ARCHITECTURE.md` | 模块架构 |
| `docs/WORLD_LORE.md` | 世界观设定 |
| `docs/D1_GAMEPLAY_LOOP_DESIGN.md` | 战斗循环设计 |
| `docs/G4_PLATFORM_BIBLE.md` | 平台兼容 + v1.0.0 Release Standard |
| `docs/V1_0_0_ACCEPTANCE.md` | 五项 Stable 验收报告 |

---

## 开发进度

| Milestone | 内容 | 状态 |
|-----------|------|------|
| M1 | CMake + Raylib 窗口 + 核心框架 | ✅ |
| M2 | 标题画面（粒子背景 + 光晕文字 + 菜单） | ✅ |
| M3 | BSP 随机地图 + 瓦片绘制 + 摄像机 | ✅ |
| M4 | 玩家 + 怪物 + AI + 战斗 | ✅ |
| M5 | 装备系统（4稀有度/武器/护甲/药水/护符） | ✅ |
| M6 | 技能系统（4主动+2被动+冷却+3级升级+时停） | ✅ |
| M7 | Boss（3种 + BossAI + 3技能 + 狂暴 + 奖励） | ✅ |
| M8 | 15层关卡 + 难度曲线 + Boss介绍 | ✅ |
| M9 | 教程（7阶段 + P跳过）+ 选关 | ✅ |
| M10 | 音频（8SFX + 4BGM 程序化合成） | ✅ |
| M11 | 存档系统（JSON 完整序列化） | ✅ |
| M12 | 日志系统（game.log + crash.log） | ✅ |
| M13 | Buff 系统（配置化/存档/HUD/玩法接入/触发统一） | ✅ |
| M14 | 特殊房间系统（祭坛/宝箱/泉水 + 发现提示 + 消息条 + Seed驱动存档恢复） | ✅ |
| M15 | 特殊房间内容深化（祭坛4结果池 / 宝箱品质分层 / 泉水净化） | ✅ |
| M16 | 特殊房间体验增强（discovered/triggered 分离 + spd存档 + 屏幕消息显示） | ✅ |
| M17 | 圣物系统 MVP（5 relic / 宝箱掉落 / 局内效果 / HUD / rlc 存档） | ✅ |
| M18 | 圣物内容扩展（11 relic + rarity + 宝箱权重掉落 + R 面板） | ✅ |
| M19 | UI 引导 + 字体覆盖稳定化（操作说明 / 快捷键提示 / 首次 relic 提示 / 混合码位） | ✅ |
| M20 | 中文显示修复：精确码位扫描替代全量CJK，字体图集从溢出变为1446码点 | ✅ |
| D1 | FloorConfig 统一配置 + FloorNarrative 15层叙事 + 章节/楼层入场演出 + 随机旁白 | ✅ |
| D2 | CombatCoordinator 连击系统 + MonsterType 6种 + ArenaObject 战场元素 | ✅ |
| D3 | BuildTag 19标签 + BuildScore 评分 + BuildType 六流派判定 + 时停E2/E3进化 | ✅ |
| D4 | EventSystem 10事件 + WorldState 标志位 + NPC/6NPC + QuestManager 7任务 + RelationshipSystem好感度 + FlowDirector动态内容 + GrowthCurve难度曲线 + RelicArchive跨局收集 + BossNarrative自适应对话 | ✅ |
| D5 | BossEvolution 技能变体/LastStand + BossBehavior 决策/人格/记忆 + BossCommand 执行层 + BossEncounter 阶段控制 + BossReplay 学习/评估 + BossTimeline 时间线 + BossCinematic 演出/Phase2 | ✅ |
| D6 | BossSystemDirector 统一编排 + GameplaySystemDirector 世界/叙事/结局 + PresentationSystemDirector 视觉表现 + GameFlowDirector 场景流程 + MetaProgression 局外永久成长 + EndingDirector 五结局 + CreditsScene 片尾 + PlayerController 输入分离 + CombatFeel 打击感 | ✅ |
| G1.1 | AttackEvolutionState + AttackEvolutionManager (普攻进化 Lv1→Lv3) | ✅ |
| G1.2 | Attack Evolution Visual Layer (剑气/旋风斩 VFX, 无Gameplay修改) | ✅ |
| G1.3 | SkillEvolutionManager (技能使用次数驱动进化) + has_confirmed_build() | ✅ |
| G1.4 | RuleChainManager (Boss死亡→规则激活→WorldState→后续楼层影响) | ✅ |
| G1.5 | EnemyDef 数据模块 + enemies.json (10 enemies 全数据驱动) + spawn_monster 通用工厂 | ✅ |
| G1.6 | BossDef 数据模块 + bosses.json (6 bosses 数据驱动) + Phase2 参数化 + Vampire 新Boss | ✅ |
| G1.7 | Save v2: atl + skill evo/use + rule_counters 序列化 + 向后兼容旧存档 | ✅ |
| G2.0 | Infrastructure Polish: 4 Def 统一接口 (get_all + is_loaded) + 重复 ID 检测 + 加载日志标准化 | ✅ |
| G2.1 | Dialogue Data Driven: dialogues.json + DialogueDef + BossNarrative 重构 | ✅ |
| G2.2 | TeamAI: TeamCoordinator + TeamDecision + MonsterAI 重构 (143→55 行) | ✅ |
| G2.3 | Boss Arena v2: BossArenaDef + ArenaEvent + execute_event() | ✅ |
| G2.4 | QuestDef + quests.json (12 quests) + EventBus quest events + Save v3 qst: | ✅ |
| G2.5 | EndingDef + endings.json + Save v3 end: + ENDING_REACHED emit | ✅ |
| G3.1 | MetaNodeDef + meta_nodes.json + MetaProgression::load_from_defs() (10 nodes) | ✅ |
| G3.2 | SkillDef + skills.json (6 skills) + SkillFactory + _skill_id 替代 dynamic_cast | ✅ |
| G3.3 | ItemDef + items.json (20 templates) + ItemFactory 替代硬编码数组 | ✅ |
| G3.4 | Architecture Freeze: 命名统一 + 12 模块 API 审计 | ✅ |
| G3.5 | Meta Reward Integration: reward_from_ending() + MetaRewardRecord 审计日志 | ✅ |
| G4.1 | Mod Loader: IRegistryProvider + RegistryBuilder + BuiltinProvider + ModProvider + 12×_from_json | ✅ |
| G4.1.5 | Registry Validator: cross-ref checks (Skill→Buff, Item→Skill, Enemy→Buff) + required fields | ✅ |
| G4.2 | Namespace ID (mod_id:entry_id) + DependencyResolver + Merge v2 (topological sort) | ✅ |
| G4.3 | Advanced Merge: MergePatch (__patch field merge) + merge_patch.h helper + BuildRecord patch | ✅ |
| G4.4 | ModManager: scan/enable/disable/list + mods/config.json + startup summary | ✅ |
| G4.5 | Replay Regression: ReplayFile + Recorder + Player + _is_action + state_hash + seed_rng + CLI | ✅ |
| G5.1 | Build Diversity: BuildType 6→12 + skills 6→20 + relics 33→63 + buffs 20→25 + items 20→36 + enemies 10→23 | ✅ |
| G5.2 | Signature Skills: IceNova/ChainLightning/ShadowStrike/BloodFrenzy/SummonSpirit | ✅ |
| G5.3 | Enemy Archetypes: AIArchetype + SNIPER/CONTROLLER/AMBUSH/GUARDIAN + enemies 23→31 | ✅ |
| G5.4 | Boss Rework: Whirlwind/LaserBarrage/GravityPull + per-boss Phase2 identity (6 unique) | ✅ |
| G5.5 | Run Events: spawn rate 25→40% + ch2+双事件 + special rooms 2-3→3-5 + NOTHING权重↓ | ✅ |
| G5.6 | Balance Pass: SimAI + SimRunner + --sim N CLI + automated 100-run balance report | ✅ |
| G5.7 | Game Feel: hit-stop + shake + freeze boost + crit scale + combo milestone juice | ✅ |
| G5.8.2 | BuildTheme: 7-field struct + 12 presets + dmg_color_for() 3-tier damage colors | ✅ |
| G5.8.3 | Camera: shake/dash offset/boss landing zoom integrated | ✅ |
| G5.8.4 | Audio Director: crossfade + boss Phase2 cue + BGM ducking | ✅ |
| G5.8.5 | VFX Recipes: vfx_recipes.json (12 recipes/11 presets) + play_recipe() | ✅ |
| G5.8.6 | Timeline: delay/duration/callback sequenced events + include() | ✅ |
| G5.8.7–8 | Presentation Integration: PresentationEvent + dispatch() + Timeline sequencing | ✅ |
| G6.1 | Biome System: 3 biomes (Prison/Volcano/Abyss) + TilePalette + enemy_pool/boss_id + biome BGM | ✅ |
| G6.2 | Landmark System: 9 biome landmarks + SpecialRoomType.LANDMARK + DungeonGenerator placement | ✅ |
| G6.3 | Biome Hazards: 6 environmental hazards (已移除 — v0.9.33 死链路清理) | ✅ |
| G6.4 | Biome Events: 6 risk/reward events (25% floor trigger) + floor_config BGM biome routing | ✅ |
| G6.5 | Encounter Framework: EncounterDef/Node/Choice + multi-round dialogue + trade + 9 encounters | ✅ |
| G6.6 | Exploration: wall_interact secrets + SpecialRoomType.SECRET 30% placement + 3 secret encounters | ✅ |
| G6.7 | Meta Progression: EncounterDef.conditions[] + pick_encounter_by_trigger() | ✅ |
| G7.1 | World Validator: tools/world_validator.py — 20+ JSON cross-ref checker, 0 errors 0 warnings | ✅ |
| G7.2 | Automated Test Framework: GoogleTest + 9 suites/43 tests + CI workflow | ✅ |
| G7.3 | Simulation & Balance: SimulationConfig + RunResult + JSON report + per-build/relic/enemy stats | ✅ |
| G7.4 | DecisionAgent Upgrade: BuildType-aware behavioral profiles + action evaluator + --sim-all-builds | ✅ |
| G8.1 | Behavior Tree: BTNode/Selector/Sequence/Condition/Action/Blackboard + BTAgent + 16 tests | ✅ |
| G8.2 | Navigation: A* pathfinder + MoveToTarget BT node + 7 astar tests | ✅ |
| G8.3 | Combat MCTS: MCTSNode + UCT search + SimulationState clone + 16 tests + --sim-ai mcts | ✅ |
| G8.4 | RL Environment: Gym-like API + Observation + RandomAgent + QAgent + 17 tests + --rl-test/train | ✅ |
| G9.0 | Weapon Framework: WeaponType(6)/HitShape(5) + WeaponDef registry + weapons.json (24 entries) | ✅ |
| G9.1 | Weapon Specials: Nunchaku 5-hit auto-track / Spear 10-hit rapid / Crossbow real projectile system | ✅ |
| G9.2 | Equipment Identity: 命名池(稀有/史诗/传奇) + Affix系统(5种) + 传奇特殊效果(5把) | ✅ |
| G9.3 | Weapon Synergy: AttackTag→技能联动 (Sword→Ice/Dagger→Shadow/Nunchaku→Lightning/Crossbow→Fire/Spear→Blood) | ✅ |
| G9.4 | VFX Overhaul: 近战全武器分阶段特效 + 远程光束/闪电 + 实体名称标签 + 品质命名重写 | ✅ |
| G10.1 | Element Core: 永久元素选择(火/冰/毒) + 元素界面 + 存档持久化 + 成长接口 | ✅ |
| G10.2 | Damage Type: AttackType 实装(PHYSICAL/MAGICAL/TRUE) + DamageResult 管道 | ✅ |
| G10.3 | Element Combat: Fire暴击/Ice减速冻结/Poison毒伤 + 元素经验 + VFX浮动标签 | ✅ |
| M4a | Boss 核心环革新样板: 暗影骑士连招机器 (弹幕/扇形斩/瞬移/旋风/召唤 combo 驱动) + 技能预警表现 + zone 伤害间隔修正 | ✅ |
| M4a.1 | M4a 战斗体验修复: 连招触发距离 48→192px / 脱战 384px 不打断连招 / 旋风范围圈可视化 / 狂暴紫色演出 / 弹幕全特效 | ✅ |
| M4a.2 | M4a 数值平衡: 毒池 DOT 改 0.5s 周期 + 高亮 / 弹幕撞墙消失 / 旋风 1.6× 扇形 1.25× / shadow_knight 专属连招配置 | ✅ |
| M4a.3 | 伤害日志全链路: attack_target 统一标签 + CombatStats 记账去重 + 每帧兜底未标注掉血 + [COMBO] 连招可见性 | ✅ |
| M4f | 美术管线: 像素渲染骨架 + 角色/怪物/VFX 全接入程序化精灵 | ✅ |
| M4f.3 | 怪物差异化体型 + 待机帧动画 (2帧 spritesheet 管线直通) | ✅ |
| M4f.4 | 数据驱动素材 — Kenney Tiny CC0 精灵上线 (玩家/怪物/墙地板) | ✅ |
| M4f.5 | Boss 形象按层 + 特殊房间装饰素材上线 | ✅ |
| M4f.6 | NPC 形象 + 武器图标素材接入 (楼层精灵映射/装备侧显) | ✅ |
| M4f.7 | 地面换 t000 暗砖(亮度50%) 墙恢复原素材 | ✅ |
| M4f.8 | 中文字符显示修复: 重生成字体码位表(+35字) 对话箭头 ▶→→ | ✅ |
| M4f.9 | 双节棍/弩武器素材 + 注册, 弩底部握持小幅摆动 | ✅ |
| M4f.10 | 双节棍素材重绘(对称双节+粗链+金属箍) + 动作改甩链 360° 旋转 | ✅ |
| M4f.11 | 全武器待机姿态微调: 匕首快颤/剑沉稳/矛挺拔/弩瞄准微调/双节棍惯性晃 | ✅ |
| M4f.12 | 怪物持械: 类型映射武器素材 + 攻击挥砍动画 | ✅ |
| M4f.14 | 特殊房间 9 素材 + 装甲/药水/护符图标, 掉落与背包图标化 | ✅ |
| M4.1 | 镜像战术层: MirrorTactic 画像+态势驱动 (远程消耗/压进近战/拉扯/平衡) + 按战术映射技能选择 | ✅ |
| M4.2 | 镜像专属真冻结: Phase≥2 冻结玩家 3s (禁移动/攻击) + 红霜 overlay | ✅ |
| M4.3 | 镜像武器槽切换: 近战(玩家武器)↔远程(CROSSBOW) 战术驱动, 独立防抖 | ✅ |
| M5 | 条件维度: 采集朝向/受击窗口 → 统计受压反击率·朝向稳定度·节奏方差 → 战术层消费 (反击型→KITE, 单向癖→远程封锁, 四面转→缠斗) | ✅ |
| M4.4 | 战术链序列记忆: 12符号 n-gram 学习玩家 技能→位移→连招 套路, 仲裁链 ML→战术链→克隆→Thompson | ✅ |
| M4.5 | 战术链跨场景: predict_next_action 链优先 (SKILL/ATTACK) + 链预测技能提前打断; E2E 真实采集链验证 | ✅ |
| Q4.1 | HitStop 修复: freeze_timer 接入主循环, 冻结期世界暂停 (打击感三件套真正生效) | ✅ |
| Q4.2 | BGM 循环重播 + stop 修 bug (停错曲目), AudioServer::update 接入主循环 | ✅ |
| Q4.3 | 拾取反馈: pickup 音效 + ring/spark 闪光 (圣物金色) | ✅ |
| Q4.4 | 受击/攻击音效: hurt + monster_atk 合成音, MONSTER_ATTACK 事件解耦 (AI 层无音频引用) | ✅ |
| Q4.5 | UI 音效 (ui_click/ui_confirm) + 标题菜单悬停高亮/点击, _activate 统一动作分发 | ✅ |
| Q4.6 | VFX recipe 消费 sfx/camera_shake 字段 (28 处死数据激活), 补 ice_crack/lightning/summon | ✅ |
| Q4.7 | 玩家受击红屏: 全屏主题 hit_flash_tint 衰减叠加 | ✅ |
| Q3.1 | 模拟器卡墙修复: BFS 全量 rect 级碰撞判定 + 搜刮重启 (圣物 5 个/50 局) | ✅ |
| Q3.2 | 模拟器卡死根治: 玩家 2s 无位移+tile 级检测→直线逃脱 + 换血保护; 怪物 5s/12s 未动自动脱卡 (boss 12s); 远距怪 (>38格) 强制吸引; 50 局批量 0 超时 | ✅ |
| Q3.3 | 模拟器搜刮修复: 清层后先搜刮未触发特殊房再下楼 (原直接下楼资源全丢) + 半格偏移卡死看门狗; 装备掉落权重 50%→75% (武/甲37.5% 药/符12.5%); F5 击杀 14%→42% F10 6%→20% 首杀4boss局; boss 数值 -15% (F5三boss) | ✅ |
| Q3.4 | 模拟器喝药+脱卡根治: sim 残血喝背包治疗药水 (1s CD, 无自愈时); 战斗中脱卡豁免 (怪在自身攻击射程内=换血/狙击, 传送打断战局→无限循环, 含弓箭手9格放风筝); F10 火魔 380→320; 玩家基础攻击 11→12; F11-14 普通怪倍率 -12%; 20局双seed验证 0 超时 F5 50% F10 25% | ✅ |
| Q3.5 | 模拟器 UAF + 原地传送修复: 挂起伤害裸指针 UAF (被清理后仍引用); 脱卡环禁止原地传送 (环内首个可行走格常=怪当前格 → 每5s传送回原格 → 900s死锁, v38 F11 4怪同格); sim run 1 在 enter_floor 前播种 rng (楼层种子确定) | ✅ |
| Q3.7 | 胜利判定修正: next>MAX_FLOORS(15) 且玩家存活才算通关 (死在 F15 不算) | ✅ |
| Q3.8 | 脱卡状态 static→实例成员: 跨局残留裸指针键清零 (run 间状态泄漏) | ✅ |
| Q3.9 | 怪物实例 ID 唯一不复用 (同种子确定) + CountingRng 掷骰计数; sim HP 统计按 instance_id 追踪/回收; 镜像冻结计时归零 (F15 冻结中死亡后跨局泄漏) | ✅ |
| Q3.11 | 模拟器通关路径修复: sim 胜利后不得执行 VictoryScene 流程 (_collect_sim_stats 已处理重启/退出) — 否则批次挂在 VictoryScene 永不退出 (镜像削弱后出胜局触发, 伪装成"无限循环冻结", v13/v14 各耗 150min); 修复后 10 局批次 ~4s 退出, 出真实胜局 (暗影刺客 F15 1091 turns) | ✅ |
| Q3.12 | F15 Mirror Baseline Evaluation (500 runs, 5 seeds): 胜率 5.8% (29/500), 死亡分布 F1-5≈40% F6-10≈50% F11-15≈8%; 中期数值下调 — F6-10 倍率 -12% (1.60→1.40/1.50→1.30 … 2.20→1.95/2.00→1.75), F2-4 微调 (-5~8%), F10 火魔 320→300; 验证胜率 ~6-10% (s7 达 10%+); 附带发现间歇性堆损坏崩溃 (ntdll 0xC0000005, ~1/3 批次, 待排查) | ✅ |
| Q3.13 | 间歇堆损坏崩溃根因定位与修复 (gdb 堆栈抓取): ① 双节棍跨帧 `sp.tracked` 裸指针 UAF (目标死亡释放后悬垂, 写已释放堆 → ntdll 随机崩溃) — 用 std::find 校验当前帧存活列表; ② SummonMinions 召唤指针 push 进 static 向量跨帧累积悬垂 (static→局部); ③ 根因: `pixel_to_tile` 截断取整, 击退/传送使实体位置出图 → BFS `is_target[ty*w+tx]` 越界写堆 (OOB 怪跳过 + 玩家瓦片钳制 ×3 BFS); 验证 8×100 局零崩溃 (s7 ×3 11%/6%/9%, s2000 ×2 7%/11%), 34/34 测试通过 | ✅ |
| Q3.14 | Sim 确定性修复 (对拍定位三处指针残留/跨进程分叉): ① 怪物脱卡状态指针键→instance_id + 换层清空 (地址复用→新怪被当"卡住已久"秒传); ② SimAI `_mem_target` 指针键→uint64 (旧记忆污染新怪→决策分叉); ③ 双节棍 `sp.tracked` 裸指针→`tracked_instance` (F1 激活跨 ~3700 帧残留到 F5, 地址复用→一进程打死史莱姆另一进程打不中, 该帧 rng 13 vs 6); 验证三种子 (500/1000/2000)×20 局×2 批逐字节一致 (130 万行级对拍), 评估基准 5%/15%/10%, 34/34 测试通过 | ✅ |
| M4.6 | RL 决策层接入镜像 Boss: 离线训练 Q 表 (95%+ 收敛) 运行时加载, 仲裁链 ML→战术链→RL→克隆→Thompson, 实测 RL 完全接管仲裁 (Tho:0), 500 局胜率 8.6%→6.6% (Boss 变强, 区间内) | ✅ |
| M4b | 第二章 Boss 领域作战 (地狱火魔): ① 弹幕图案化 (扇形/环形/螺旋多波, 波次发射, 帧率无关步进) ② 机制阶段 MECHANIC_PHASE 激活 (核心破坏→弹幕风暴演出→易伤, 遭遇阶段驱动连招模板) ③ Boss 房机制地形 (LAVA 地砖灼烧 + 中央安全区熔岩环带, 数据驱动); 500 局胜率 7.0% 区间内, 34/34 测试 | ✅ |
| v1.0.0 | Release Standard 五项 Stable 冻结验收: API (对外契约 2+ 版冻结) / Save (v1→v3 兼容 + 3 验收测试 + 修复 elem 存档丢失 bug) / Mod (管线+测试) / Regression (确定性对拍 + 500 局 8.0%) / Performance (500 局并行 53s) — 报告 docs/V1_0_0_ACCEPTANCE.md | ✅ |
| 收官体检 | 全仓代码体检修复: ① 删除 hazards.json 死链路 (零消费者) ② EXPLOSIVE_BARREL 最小闭环 (攻击/弹体点燃 → 0.6s 引信 → AOE 爆炸 → VFX → 销毁, 实测 38 次成对) ③ Boss 技能冷却恢复判定 (can_use 读端接入, 冷却中退普攻) ④ bgm 音符解析警告修复; 34/34 测试, validator 0 error, 500 局 10.0% 区间内 | ✅ |