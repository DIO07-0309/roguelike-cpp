# 地牢肉鸽 — Roguelike C++

> C++17 + Raylib 5.0 | CMake | ~260 源文件 | Windows / macOS / Linux
> 
> 随机生成 15 层地牢，击败 Boss「深渊之主·终焉」通关。

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

## 武器系统 (G9)

6 种武器，各 3 段连击，5 种命中判定形状。品质越高伤害越高、特效越强，传奇武器有专属效果。

| 武器 | 距离 | Stage 1 | Stage 2 | Stage 3 | 判定 |
|------|------|---------|---------|---------|------|
| **拳头** | 1× | 直拳 | — | — | 圆形 |
| **匕首** | 1× | 横斩 | 竖劈 | 突刺 1.5× | 扇形→扇形→胶囊 |
| **长剑** | 2× | 跳斩 | 横扫 | 震地+眩晕 | 矩形→扇形→胶囊 |
| **双截棍** | 3~5× | 鞭击 | 反身抽 | 5-hit 自动追踪 | 胶囊 |
| **连弩** | 10× | 单箭 | 三连箭 | 蓄力箭 穿墙 | 弹道 |
| **长矛** | 6× | 突刺 | 挑击+击退 | 传锋 10-hit | 矩形→矩形→扇形 |

### 品质命名

| 品质 | Dagger | Sword | Nunchaku | Crossbow | Spear |
|------|--------|-------|----------|----------|-------|
| 普通 | 匕首 | 长剑 | 双截棍 | 连弩 | 长矛 |
| 稀有 | 暗影猎手/血牙 | 破军剑/苍炎剑 | 铁流双节/玄木双棍 | 迅影弩/寒星弩 | 追风枪/烈阳枪 |
| 史诗 | 夜魔之刃/深渊獠牙 | 天罡剑/赤霄 | 雷鸣双节/破风棍 | 天机弩/破晓弩 | 苍龙枪/破军长矛 |
| 传说 | **恶魔之爪** | **倚天剑** | **李小龙** | **东风破** | **惊破天** |

### 传奇效果

- **倚天剑** — 震地冲击波范围 ×1.3
- **恶魔之爪** — Stage-3 100% 附加中毒
- **李小龙** — 连击次数 +2 (5→7)
- **东风破** — 蓄力箭伤害 ×1.5
- **惊破天** — 传锋次数 +2 (10→12)

### Boss 掉落

| 楼层 | Boss | 掉落 |
|------|------|------|
| F5 | 暗影骑士 | 惊破天 |
| F10 | 地狱火魔 | 东风破 |
| F15 | 深渊之主·终焉 | 倚天剑 |

---

## 技能

共 20 个主动技能（4 基础 + 16 变体），6 个被动技能。

### 基础技能

| 技能 | CD | 效果 |
|------|-----|------|
| **斩击** | 2s | 前方锥形近战 |
| **神罚** | 5s | 远程 AOE + 减速 |
| **自愈** | 8s | 回血 + 攻击提升 |
| **The World** | 20s | 时停 |

新游戏首技能必为以上之一，后续升级随机学习变体。

---

## 元素核心系统 (G10)

新玩家首次创建角色时选择永久元素核心，选后不可更改。

### 三种元素

| 元素 | 机制 | Lv1 数值 | Lv20 数值 |
|------|------|----------|-----------|
| **🔥 火焰** | 攻击概率暴击，伤害×1.5 | 暴击率 15% | 暴击率 ~30% |
| **❄ 冰霜** | 每击附加减速，层数满或概率触发冻结(1秒) | 冻结率 10% | 冻结率 ~100% |
| **☠ 剧毒** | 每击附加毒伤，DOT = 本次伤害×比例 | 毒伤 5% | 毒伤 ~15% |

### 元素 VFX

| 效果 | 火 | 冰 | 毒 |
|------|-----|-----|-----|
| 普通攻击 | 橙红爆炸 + 火花 + `[火]` 标签 | 蓝色光束 + 三层冰圈 + `[缓]` 标签 | 绿色光束 + 毒雾 + `[毒]` 标签 |
| 暴击/冻结 | 大爆炸 + 冲击波 + 震屏 + `[暴击!]` | 蓝白闪光 + 冲击波 + 冻结帧 + `[冻!]` | — |
| DOT 跳伤 | — | — | 毒环 + 绿色粒子 |

### 其他 G10 优化

- Boss 掉落的圣物**不会在换层时被清除**，跟随玩家进入后续关卡
- 圣物刷新房间**靠近玩家出生点**，开局即可获取
- 敌方头顶显示 **buff 标签**（毒/缓/冻/血/燃/晕/惧/雷/防），14px 彩色大字加阴影
- 元素选择界面**每张卡片内含详细机制说明和成长数据**
- 伤害类型系统：武器用 PHYSICAL，长矛传锋用 MAGICAL，未来 TRUE 可穿透防御

---

## 敌人

31 种敌人，9 类 AI。Boss 有专属技能 + 狂暴机制。

| 楼层 | 主题 | 池 |
|------|------|-----|
| F1–5 | 遗忘监狱 | 骷髅弓手/骨兵/史莱姆/暗影行者 |
| F6–10 | 灰烬火山 | 火妖/精英兽人/冲锋兽人 |
| F11–15 | 虚空深渊 | 暗术师/虚空行者/石像守卫 |

**三场 Boss 战**：暗影骑士(F5) / 地狱火魔(F10) / 终焉回响(F15)

---

## 智能 AI 系统 (G8 + F15)

本项目是面向**游戏 AI 研究**的完整实验平台。F5 暗影骑士考验反应，F10 地狱火魔考验规则理解，**F15 终焉回响让 AI 学习你的习惯并用你的方式击败你**。

### 架构全景

```
Player Behavior Pipeline (F15.1-F15.2)
  记录 14 层行为 → PlayerAction 事件流
       │
       ▼
PlayerHabitProfile (F15.3)        G8.1 Behavior Tree
  玩家画像：风格/频率/弱点           基础决策：追击/防御/撤退
       │                                    │
       ▼                                    ▼
MirrorAgent (F15.3-F15.4)          G8.3 Combat MCTS
  反制策略 + 动作预测               100 次模拟 → 最优动作
       │                                    │
       ▼                                    ▼
  RL Self-play (F15.4)             G8.4 Q-Learning Agent
  --rl-mirror 500                 离散化状态 → Q-table 训练
```

### F15 终焉回响 — 从技术到体验

| 阶段 | 技术 | 玩家感受 |
|------|------|----------|
| **开场** | PlayerBehaviorAnalyzer 读取 14 层数据 | 面板弹出：「风格: AGGRESSIVE，弱点: 低闪避」— *它认识我* |
| **Phase 1** | MirrorAgent 实时观察当前战斗 | Boss 复制你的武器/技能/装备，*我在打自己* |
| **Phase 2** | 历史反制策略激活 | Boss 开始预判你的技能释放、封堵闪避方向 — *它开始预测我* |
| **Phase 3** | 进化版人格 | *它比我更懂我* |

> **M4 战术层 (v0.9.21)**：镜像新增画像驱动的战术脚本层 —— 根据你的攻击习惯
> 在远程消耗 / 压进近战 / 拉扯之间切换（含武器槽同步切换），并在 Phase 2 起
> 使用镜像专属真冻结（玩家 3 秒内无法移动/攻击，镜像仍可行动）。

### AI 技术清单

| 系统 | 文件数 | 说明 |
|------|--------|------|
| **Behavior Tree** | 5 | 选择器/序列/条件/动作/黑板，14 个节点，决策时延 <1ms |
| **A* Pathfinder** | 2 | priority_queue + Manhattan 启发式，有障碍物寻路 |
| **Combat MCTS** | 3 | UCT 搜索 + 战斗快照 clone，100 次模拟预测最优动作 |
| **Q-Learning Agent** | 2 | Observation 7 维向量 → Q-table 离散化，ε-greedy 探索 |
| **Player Behavior Recorder** | 4 | 14 层静默采集：武器攻击/技能/移动/闪避/受伤，每局 260+ 事件 |
| **Player Behavior Analyzer** | 2 | 事件流 → 玩家画像：频率/偏好/风格聚类 → 反制策略生成 |
| **MirrorAgent** | 2 | 3 阶段人格演化 + reward 函数：伤害 + 预判 + 反制成功 |
| **2nd-Layer Boss Domain** | 4 | BossArenaState 状态机 + WeakPoint + 无敌/易伤循环 |

### CLI 训练命令

```bash
# 离线 RL 训练：MirrorAgent × 4 玩家风格 × 500 局
build/roguelike_cpp --rl-mirror 500

# 行为树模拟
build/roguelike_cpp --sim-ai bt

# MCTS 模拟
build/roguelike_cpp --sim-ai mcts

# Q-learning 训练
build/roguelike_cpp --rl-train 1000
```

---

## 房间

每层 2~5 个特殊房间：祭坛、宝箱、泉水、商店、铁匠、图书馆、赌徒、神殿、隐藏密室。

---

## 项目结构

```
src/
├── core/          # 引擎框架 (Object/Node/SceneTree/InputMap)
├── game/
│   ├── entities/  # 实体 (Player/Monster/Item/Skill/Inventory)
│   ├── systems/   # 战斗/武器/VFX/楼层
│   ├── world/     # 地图/地牢生成/特殊房间/事件/NPC
│   ├── scenes/    # 场景 (Title/Game/Tutorial/Death/Victory)
│   ├── director/  # 表现层/游戏流程/Boss系统
│   ├── audio/     # 程序化合成音频
│   └── save/      # 存档
├── data/          # JSON 加载器 (items/buffs/enemies/skills/weapons…)
├── ai/            # 行为树/导航/MCTS/RL
└── tests/         # GoogleTest (80+ 用例)
resources/         # JSON 配置（12+ 文件）
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
| `--record <path>` | 录制 Replay |
| `--replay <path>` | 回放 |
| `--sim N` | 跑 N 局模拟 |
| `--sim-ai bt/mcts` | 行为树/MCTS 模拟 |
| `--rl-test N` | RL 环境测试 |

### Python 工具

```bash
python tools/world_validator.py    # JSON 交叉校验
python tools/extract_chars.py      # 提取 CJK 字符
```

### 设计文档

| 文档 | 内容 |
|------|------|
| `docs/ARCHITECTURE.md` | 模块架构 |
| `docs/WORLD_LORE.md` | 世界观设定 |
| `docs/D1_GAMEPLAY_LOOP_DESIGN.md` | 战斗循环设计 |
| `docs/G4_PLATFORM_BIBLE.md` | 平台兼容 |

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
| G6.3 | Biome Hazards: 6 environmental hazards on landmark rooms (slow/burn/confuse/deflect) | ✅ |
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
| M4b | 第二章 Boss 领域作战 (茶杯头式 Boss 房 / 弹幕演出 / 机制阶段) | ⏳ 开发中 |
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
