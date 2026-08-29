# 地牢肉鸽 — Roguelike C++

> **v1.0.0 正式版** — C++17 + Raylib 5.0 | CMake | **297 源文件** | Windows 实机验证
>
> 随机生成 15 层地牢，击败 Boss「终焉回响」通关。
> 五项 Stable 冻结验收通过：**API / Save / Mod / Regression / Performance**（报告 `docs/V1_0_0_ACCEPTANCE.md`）
>
> **v1.0.0 后新增**（Phase 1-3 + Batch A-H，见 CHANGELOG）：FOV 可见性系统 · Room→Door→Corridor 地牢拓扑 · Minimap 小地图 · 碰撞/视觉分离 · E 键门交互 · 门视觉动画 · Boss FOV · 弹幕墙体碰撞 · **经济系统（金币/钥匙/圣物持久化/背包出售）· 赌徒房 · 挑战房（传送门 + 选择面板 + 波次战斗 + 独立竞技场）**

本项目定位：

1. **完整可玩的游戏** — 3 章 15 层、5 场 Boss 战、武器/技能/元素/圣物/局外成长全链路
2. **数据驱动架构** — 20+ JSON 配置 → Registry → 运行时，Mod 可热插拔
3. **游戏 AI 研究平台** — 行为树 / MCTS / Q-Learning / 镜像学习 Boss，`--sim` 批量评估

---

## 快速开始

### 方式一：即玩包（推荐，无需任何环境）

GitHub Releases 下载 **`roguelike-cpp-v1.0.0.zip`**（[Release 页](https://github.com/DIO07-0309/roguelike-cpp/releases)）→ 解压 → 双击 `roguelike_cpp.exe` 直接游玩（exe + 全部素材/配置已打包）。

### 方式二：源码构建（第三方库已入库，clone 即可编译）

```bash
git clone https://github.com/DIO07-0309/roguelike-cpp.git
cd roguelike-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/roguelike_cpp.exe
```

环境要求：CMake 3.16+ + MinGW-w64 工具链（UTF-8 编译标志已内置于 CMakeLists）。`vendor/`（raylib 5.0 + nlohmann/json）已随仓库提供，无需额外下载。

> 本地开发目录同样适用：`C:\Demo\roguelike_cpp` 构建产物位于 `build/`。

---

## 操作

### 移动与战斗

| 按键 | 功能 |
|------|------|
| **WASD / ↑↓←→** | 移动（八方向，斜向 √2 归一化） |
| **空格** | 普攻（武器三连击） |
| **1~4 / 小键盘 1~4** | 主动技能 |
| **E** | 拾取 / 触发特殊房间 / 开门 / 下楼 |
| **B** | 背包（↑↓/WS 选择 · ←→ 翻页 · X 装备 · T 出售 · U 使用 · D 丢弃） |
| **R** | 圣物面板 |

### 系统

| 按键 | 功能 |
|------|------|
| **N** | 新游戏 |
| **C** | 继续 |
| **F** | 选关 |
| **T** | 教程 |
| **F1** | 事件日志 |
| **M** | 小地图开关 |
| **Enter** | 确认（对话/菜单） |
| **Esc** | 取消 / 返回标题 |
| **G** | 全屏切换 |

> 按键映射唯一来源：`src/core/input_map.cpp:46`（`setup_defaults`）— 与代码同步维护。

---

## Feature 总览

### 核心循环（实时动作）

即时制战斗：攻击间隔 0.5s，HitStop 打击感 + 震屏 + 冻结帧 + 连击评分；楼内清怪 → 下楼 → 强化 → Boss。死亡即重开（肉鸽），局外 MetaProgression 10 节点永久成长。

### 地牢生成

BSP 二分划分随机地图，3 章 × 5 层（F1-5 地牢入口 / F6-10 幽暗深渊 / F11-15 虚空深渊），F4/9/14 休整层、F5/10/15 Boss 层。**Room→Door→Corridor 拓扑（Phase 2）**——房间经边缘门与走廊连接，走廊只雕刻墙壁不侵入房间内部；门支持 4 种状态视觉（Kenney 素材 tile_0003/0018/0022）：OPEN 空拱门 / CLOSED 木门 / LOCKED 木门+红色锁 / SEALED 深色门+紫色封印，0.3s 过渡动画。11 类特殊房间（祭坛/宝箱/泉水/商店/铁匠/图书馆/赌徒/神殿/隐藏密室/地标/挑战房）+ ArenaObject 战场元素（毒池/尖刺/图腾/**木桶 — 攻击或弹体点燃 → 0.6s 引信 → AOE 爆炸**）。F10 地狱火魔 Boss 房含机制地形（LAVA 灼烧 + 中央安全区熔岩环带）。

### 战斗系统

- **武器 (G9)** — 5 类 × 3 段连击（匕首/长剑/双截棍/连弩/长矛）× 5 种命中形状（CIRCLE/SECTOR/RECTANGLE/CAPSULE/PROJECTILE），21 条 JSON 配置，传奇特效 + 攻击标签联动
- **技能** — 22 条：16 主动（4 基础：斩击/神罚/自愈/The World + 5 签名：冰爆/连锁闪电/暗影突袭/血怒/召唤英灵 + 7 数据驱动变体）+ 6 被动，每技能 3 级进化（使用次数驱动）
- **元素核心 (G10)** — 开局永久三选一：火焰暴击 / 冰霜冻结 / 剧毒 DOT，独立成长 + VFX
- **Boss** — 5 场（F5 三选一）：暗影骑士 / 亡灵法师 / 血族伯爵 / **地狱火魔(F10)** — 弹幕图案化（扇形/环形/螺旋多波）+ 机制阶段（核心破坏→弹幕风暴→易伤）+ 领域地形 / **终焉回响(F15)** — 镜像学习 Boss
- **敌人** — 30 种，9 类 AI（含 SNIPER/CONTROLLER/AMBUSH/GUARDIAN 原型），全数据驱动
- **弹幕墙体碰撞** — 玩家/怪物弹幕碰墙销毁，Boss BarrageSkill 独立碰撞，弩箭 power shot 可穿墙

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

- **FOV 可见性（Phase 1）** — 360° 射线投射，Tile 三层状态（未探索黑色 / 当前可见全亮 / 已探索不可见 60% 暗）；实体中心 tile 可见性剔除；`is_visible`/`is_explored` 独立于 `is_walkable`
- **Boss FOV** — 每帧追踪 Boss 位置，射线投射更新 Boss 可视范围，主地图红色叠加 + 小地图暗红/红色调显示
- **Minimap 小地图（Phase 3）** — 右下角常驻面板（M 键开关），只读现有 `isExplored/isVisible` **无第二套探索状态**；未探索区不显示、已探索永久记忆、当前可见高亮；Boss 视野区域红色调显示；Room/Corridor/Door 缩略色块 + 玩家/Boss 最后已知位置/楼梯标记；实体仅当前可见才显示，绝不泄露未探索区
- **存档 v4** — 跨版本兼容（v1→v4 + 增量字段），SaveStable 3 验收测试
- **经济系统** — 金币（装备出售获取）/ 钥匙（赌徒房掉落）/ 圣物持久化（FLOOR/RUN 两种作用域）/ 背包出售 [T]
- **赌徒房** — 金币开房（40+floor×10），75% 装备 / 20% 钥匙 / 5% RUN 圣物，耗尽回退钥匙
- **挑战房** — 钥匙开启，3 波 × 4 怪物波次战斗，ChallengeModifier（HP×1.5 ATK×1.3），通关 3×RARE+ 奖励
- **Mod 系统** — `mods/` 扫描 + 依赖解析 + MergeMode{Skip/Replace/MergePatch} 字段级合并
- **中文 UI** — 生成字体图集全中文渲染，`tools/extract_chars.py` 自动维护码点
- **音频** — 程序化合成（wave_synth）：14 SFX（hit/hurt/melee/slash/bolt/heal/timestop/domain_expand/victory/ui 等）+ 5 BGM（title/select/dungeon/boss/victory + biome 动态变体）+ 交叉淡入 + Boss Phase2 cue
- **回放/确定性** — Replay 录制 + hash 链逐帧校验（`--record/--replay`）
- **批量评估** — `--sim N` headless 模拟 + 平衡报告（`reports/balance_report.json`）

---

## 技术栈与专业技术

### 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17（`unique_ptr` / `shared_ptr` / `std::optional` / `enum class`），组合优于继承，函数 ≤40 行规范 |
| 图形/输入 | Raylib 5.0（窗口/绘制/输入/音频），sprite atlas + 程序化像素占位 |
| JSON | nlohmann/json（header-only），20+ 配置文件全数据驱动 |
| 构建 | CMake 3.16+ + CMakePresets + MinGW（UTF-8 编译标志），Release/Debug + 测试三配置 |
| 测试 | GoogleTest（52 ctest 条目）+ GitHub Actions CI + `world_validator.py` 数据校验 |
| Python 工具 | `tools/`：world_validator（JSON 交叉引用）/ extract_chars（中文字体码点） |

### 工程与架构技术

- **数据驱动 + Def/Runtime 分离** — JSON 不可变配置 ≠ C++ 可变状态；Registry 只读查询（`load_/get_/get_all_/is_loaded` 统一 API）；20+ JSON → 12 模块加载器 → 运行时
- **Mod 热插拔** — `IRegistryProvider` 优先级链 + MergeMode{Skip/Replace/**MergePatch** 字段级合并} + 依赖**拓扑排序** + 循环检测 + `mod_id:entry_id` 命名空间隔离
- **事件驱动解耦** — EventBus 45 事件、轻量载荷 `{type,sender,int,float,str}`、按 owner 批量注销；Gameplay→EventBus→Presentation 单向流（Gameplay 不引用 UI）
- **组合式 Director** — GameScene 组合 5 Director：BossSystem（12 子系统）/ GameplaySystem（world_state/quest/ending）/ PresentationSystem（shake/freeze/BuildTheme）/ GameFlow（12 态生命周期）/ Flow（动态内容编排）；CameraDirector 常量 + EndingDirector 判定为辅助模块；零继承
- **确定性游戏技术** — `CountingRng`（mt19937 + 掷骰计数）· 种子公式 `seed_start + run*1234567` · **replay hash 链**逐帧校验（mixer 黄金比例常量）· 指针键 → instance_id 防跨进程分叉（Q3.14 对拍逐字节一致）
- **存档兼容工程** — v1→v4 追加式字段 + `getV` 默认值 + 旧技能名映射 + SaveStable 3 验收测试；三份数据独立：save.json（局内）/ meta_save.json（局外成长）/ relic_archive.json（收藏）
- **内存安全实践** — 全智能指针 + 工厂方法（`spawn_monster`/`boss_factory_create`），无裸 `new`；SEH 异常捕获 → crash.log
- **中文字体管线** — `extract_chars.py` 精确码位扫描（1769 码点）→ 生成字体图集 → `GuiFont::DrawTextCH()`（Raylib DrawText 不支持中文）

### 算法与 AI 技术

- **BSP 二分划分** 随机地牢 + Seed 驱动确定性（同种子同地图，跨进程可对拍）
- **行为树** — Selector/Sequence/条件/动作 + 黑板（BTAgent 根 Selector 8 节点优先序）
- **MCTS** — UCT 搜索（C=1.414，100 迭代，深度 10，奖励 sigmoid 归一化）+ SimulationState 深克隆
- **Q-Learning** — 观测离散化 Q 表 + epsilon 退火 0.12→0.005 + 学习率按访问衰减 + 终局 done 处理 + RL 自博弈（95%+ 收敛）
- **n-gram 序列建模** — 11 符号 3-gram 计数表（键 `s0*121+s1*11+s2`）+ 2-gram 分母降级链
- **Thompson 采样** — Beta 后验多臂赌博机在线学习（命中奖励持续更新 + 全臂折扣遗忘防先验爆炸）
- **状态桶离散化** — `d<距离>:h<血量>:s<技能>` 三维聚类 → 行为克隆预测（exact→fuzzy→profile→default 降级）
- **A\***（priority_queue + Manhattan 启发式）生产测试双用 + **BFS** 危险避让（熔岩/毒池/尖刺/木桶）
- **Headless 确定性模拟器** — 定步长 1/60 批量评估（500 局 53s，9.4 局/s/核），胜率/死亡分布/Build 评级自动报告

### 表现与内容技术

- **程序化音频合成** — wave_synth 波形合成 14 SFX + 5 BGM（零音频素材），交叉淡入 + Boss Phase2 cue
- **VFX 图元系统** — 10 基础图元（ring/beam/lightning/explosion/slash/smoke/spark/aura/flash/shockwave）+ JSON recipe 派发 + BuildTheme 主题调制（VFX/Camera/ScreenFX/Audio 四类）
- **打击感工程** — HitStop 冻结帧 / 震屏 / 伤害数字 / 受击红闪 / 连击评分（CameraDirector 常量调参）
- **数值验证流水线** — 胜率目标区间 6-10% + 500 局回归 + 死亡分布监控 + World Validator 4 类检查

> 分层依赖图与边界规则等权威架构参考：`docs/ARCHITECTURE.md`（唯一权威文档）

---

## AI 架构

### 玩家侧决策（模拟器驱动）

| 系统 | 实现 | 细节 |
|------|------|------|
| **DecisionAgent** | 评分式决策（`src/core/sim/sim_ai.cpp`） | attack/skill/move/pickup/heal 五类计分取最大；BuildType 12 流派感知（攻击/走位/搜刮权重随流派调整）；`--sim-ai decision`（默认） |
| **BTAgent** | 行为树（`src/ai/agents/bt_agent.cpp` + `src/ai/behavior_tree/` 节点库） | 根 Selector 8 子节点优先序 — BossIntro→确认 / Stairs→下楼 / 低血→自愈 / BossNear→攻击 / AoE / EnemyNear / 拾取 / Wander 兜底；BTNode/Selector/Sequence/Condition/Action/Blackboard 结构，16 测试 |
| **MCTS** | UCT 搜索（`src/ai/mcts/`） | C=1.414，100 迭代，深度上限 10，奖励 sigmoid 归一化，终局 ±1000；SimulationState 深克隆模拟对局（含真实冷却快照）；16 测试；`--sim-ai mcts` |
| **Q-Learning** | RL 环境（`src/ai/rl/`） | Gym-like API（reset/step/reward + done）+ 观测离散化 Q 表（学习率按访问衰减）+ RandomAgent/QAgent；17 测试；`--rl-test/train` |

### MirrorAgent 详解（F15 镜像学习核心）

定位：**分析层非控制层** — 读玩家习惯调整 BossAI 参数，不直接调用 attack/move（`mirror_agent.h:21`）。三阶段人格：

| 阶段 | 进入条件（`mirror_tuning.h` 全部参数） | 行为 |
|------|------|------|
| **P1 观察** | — | 实时采集：攻击/技能 0.5s 窗口、位移、喝药识别；Boss 复制玩家武器/技能 |
| **P2 镜像** | 观察 ≥20 次 或 40 次兜底 / 时间 12s 兜底 / 准确率 ≥0.65 | 克隆预测（BehaviorCloneTable）+ 战术链 n-gram 反制 |
| **P3 进化** | 同桶命中 10 次 / 准确率 ≥0.70 / 玩家 HP <0.35 危险线 | 在线学习：reward=命中，Thompson 多臂持续探索 |

- **决策接口**：`recommend_action`（Thompson 采样，phase<2 返回 -1）/ `report_outcome`（Beta 后验更新）/ `should_interrupt_skill` / `should_pressure_close` / `predict_next_action` / `recommend_distance`
- **跨局记忆**：alpha+beta 各 36 float（9 桶 × 4 臂，桶-major，共 72）→ 存档 `mra:`/`mrb:` → 新局旧后验叠加为先验（`inject_mirror_memory`）；导出时扣除本局画像先验 + 全臂折扣遗忘 λ=0.995 防止后验无界增长（v0.9.34 MP1）
- **漂移自适应**：策略漂移 >0.5 时克隆置信门槛 0.50→0.75（步进 0.25）
- **战斗快照** MirrorBattleState：boss_hp_pct / player_hp_pct / dist_tiles / player_attacking / player_using_skill / boss_can_attack / boss_in_domain / player_skills_ready

### 仲裁链（镜像 Boss 决策，五层）

```
ML 插槽(默认关) → 战术链(n-gram) → RL(Q 表 exploit) → 克隆(行为预测) → Thompson 采样 → 规则兜底
```

| 层 | 数据结构 | 机制 |
|------|------|------|
| ML | 预测器插槽 `set_ml_predictor` | 默认关闭，未训练 |
| 战术链 | TacticalChainTable（`tactical_chain_table.h`） | 11 符号（SKILL_0-3 / MOVE_×4 / COMBO_1-3）3-gram 计数表（键 `s0*121+s1*11+s2`）+ 2-gram 分母 + 单前缀；降级链 3-gram→2-gram→1-gram |
| RL | QAgent（4 风格 Q 表） | 离线训练 95%+ 收敛，运行时按玩家画像加载，exploit 为主 |
| 克隆 | BehaviorCloneTable（`behavior_clone_table.h`） | PlayerIntention 7 类（ATTACK/SKILL/DODGE/HEAL/ADVANCE/RETREAT/IDLE）；CloneContext 状态桶 `d<dist>:h<hp>:s<skills>` — dist 5 档（<2/2-4/4-8/8-14/≥14）· hp 4 档（<25%/25-50%/50-80%/≥80%）· skill 0-3；降级链 exact→fuzzy→profile→default |
| Thompson | OnlineAdaptivePolicy | Beta 后验采样，`report_outcome` 持续更新 + 全臂折扣遗忘 λ=0.995（非平稳环境恢复探索） |

实测仲裁分布（v0.9.30，500 局）：`[Clone:0 ML:0 RL:11/25/26 Tho:0]` — RL 完全接管。

### RL 训练管线

> v0.9.34 起训练管线含终局 done 处理 + 学习率衰减（B1/B2）与增量击杀奖励（B3）；下列收敛数字为旧管线历史产物，重训后数值会变化。

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

- 镜像记忆 alpha+beta 各 36 float（9 桶 × 4 臂）写入存档 `mra:`/`mrb:` 字段（M4e）
- 新局开始自动注入 → 旧后验叠加为先验（`inject_mirror_memory`）；导出扣除当局画像先验 + 折扣遗忘，防后验无界增长（v0.9.34）
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
| v0.9.34 | **9.0%** | AI 审查修复 11 处算法 bug（BT 接线/Q-Learning done+学习率/Thompson 先验爆炸/MCTS 归一化，报告 docs/AI_LEARNING_GUIDE.md） |

### 确定性

- **Q3.14 对拍**：3 种子 × 20 局 × 2 批 **逐字节一致**（130 万行级）
- 修复三类跨进程分叉：怪物脱卡状态/SimAI 记忆/双节棍追踪 指针键 → instance_id/uint64
- Replay hash 链（`compute_state_hash` 逐帧链式 + `verify_hash_chain`）

### 性能

- sim 500 局（5 进程并行）：**53s**，单核 ~9.4 局/s（支撑万局规模评估）
- 全量测试套件：0.46s

### 测试与验证

- **52 个 ctest 条目全绿**（含 SaveStable 3 验收测试：v1 旧档兼容 / 坏条目容错 / 全字段 roundtrip）
- World Validator：20+ JSON 交叉引用 **0 errors 0 warnings**
- 木桶闭环实测：200 局 sim **38 次 点燃→爆炸 完全成对**（伤害随楼层缩放）
- 收官体检：编译 0 警告（bgm narrowing 修复后）

---

## Current Limitations

诚实清单（v1.0 已知边界）：

- **美术** — 程序化像素 + Kenney CC0 占位素材，无完整商业美术资产
- **手感** — 实时动作（攻击间隔 0.5s），无翻滚/无锁定，非回合制；打击感三件套已就位但数值打磨以研究平衡为主
- **平衡** — 胜率目标区间 6-10%（研究平台定位，非商业难度曲线）
- **Boss 决策链** — D5 决策系统 → `boss_decision_to_command` → `boss_execute_command` 完整链路（含攻击/技能/移动/领域命令），同时 HUD 展示决策名称
- **导航** — 运行时模拟用 BFS（`_bfs_toward/_bfs_away`）；A* pathfinder 已接入 BT Agent 的 MoveToTarget 叶节点（`bt_move_to_target.h`）
- **环境物** — 原 hazards.json（环境危险物）为死链路已移除（v0.9.33）；当前环境机制为 ArenaObject（毒池/尖刺/图腾/木桶）+ 熔岩地砖
- **中文渲染** — 依赖生成字体图集（1769 码点），新增中文文案需重跑 `extract_chars.py`
- **平台** — Windows 实机验证；macOS/Linux 构建规范见 `docs/G4_PLATFORM_BIBLE.md`，未实机验证
- **输入** — 键盘 only，无手柄/触摸
- **RL 资产** — Q 表生成于本地 `saves/`（训练产物，非仓库内置）；首次运行无 Q 表时镜像自动降级
- **性能** — 单线程模拟（9.4 局/s/核），万局评估需多进程并行（已验证）

---

## 项目结构

```
src/                                    # 297 源文件（138 cpp + 159+ h）
├── main.cpp                            # 入口：CLI 解析 / Registry+Mod 构建 / sim 前置 / 场景树启动
│
├── core/                               # 引擎框架 + 模拟器 + 回放 + Mod 管线（27）
│   ├── object.h                        # 引擎对象基类
│   ├── node.cpp · node.h               # 场景树节点（process/render 生命周期）
│   ├── scene_tree.cpp · scene_tree.h   # 场景树：节点挂载/卸载/逐帧驱动
│   ├── input_map.cpp · input_map.h     # 按键映射唯一来源（WASD/空格/E/B/1-4/G/Enter/Esc）
│   ├── logger.cpp · logger.h           # 分级日志（game.log）
│   ├── seh_handler.cpp                 # Windows SEH 异常捕获 → crash.log
│   ├── win_center.cpp · win_center.h   # 窗口居中
│   ├── font_codepoints.h               # 中文字体码点表（1835 码点）
│   ├── registry_provider.h             # IRegistryProvider 接口 + MergeMode{Skip/Replace/MergePatch} + BuildRecord
│   ├── registry_builder.cpp · registry_builder.h   # 注册表构建：模块注册 / build_all / validate
│   ├── builtin_provider.cpp · builtin_provider.h   # 内置数据源（resources/ 路径，priority 0）
│   ├── mod_provider.cpp · mod_provider.h           # Mod 数据源（priority 来自 mod.json，默认 100）
│   ├── mod_manager.cpp · mod_manager.h             # Mod 扫描/启用/禁用（mods/config.json）
│   ├── mod_dependency.cpp · mod_dependency.h       # Mod 依赖拓扑排序 + 循环检测
│   ├── merge_patch.h                               # MergePatch 字段级递归合并（__patch 标记）
│   ├── sim/
│   │   ├── sim_ai.cpp · sim_ai.h       # DecisionAgent 评分决策（--sim-ai decision）+ BFS 危险避让
│   │   └── sim_runner.cpp · sim_runner.h           # 批量模拟：RunResult / BalanceReport / 种子公式
│   └── replay/
│       ├── recorder.cpp · recorder.h   # 回放录制（--record）
│       ├── player.cpp · player.h       # 回放播放（--replay）
│       ├── replay_file.cpp · replay_file.h         # 回放文件读写格式
│       └── state_hash.cpp · state_hash.h           # 状态 hash 链（compute/verify_hash_chain）
│
├── data/                               # Def 加载器：JSON → 不可变配置（22）
│   ├── boss_defs.cpp · boss_defs.h     # Boss 配置（HP/ATK/Phase2 参数/技能变体）
│   ├── enemy_defs.cpp · enemy_defs.h   # 敌人配置（30 种：属性/AI 原型/掉落）
│   ├── skill_defs.cpp · skill_defs.h   # 技能配置（22 条：伤害/冷却/3 级进化）
│   ├── item_defs.cpp · item_defs.h     # 物品配置（31 模板：装备/消耗品/护符）
│   ├── weapon_defs.cpp · weapon_defs.h # 武器配置（21 条：三连击/命中形状/特技）
│   ├── element_defs.cpp · element_defs.h           # 元素配置（火/冰/毒：成长曲线）
│   ├── quest_defs.cpp · quest_defs.h   # 任务配置（12 条：条件/奖励/自动解锁）
│   ├── ending_defs.cpp · ending_defs.h # 结局配置（5 结局）
│   ├── dialogue_defs.cpp · dialogue_defs.h         # 对话配置（34 条：Boss 自适应条件）
│   ├── meta_node_defs.cpp · meta_node_defs.h       # 局外成长节点（10 天赋）
│   └── vfx_recipe.cpp · vfx_recipe.h   # VFX 特效配方（13 kind / 17 color）
│
├── game/                               # 游戏逻辑（195）
│   ├── 顶层
│   │   ├── player_controller.cpp · player_controller.h     # 玩家输入集中：移动/攻击/技能/拾取/背包
│   │   ├── meta_progression.cpp · meta_progression.h       # 局外成长 g_meta（10 节点/souls/knowledge）
│   │   ├── relic_progression.cpp · relic_progression.h     # 跨局圣物收藏 g_relic_archive（mastery/套装）
│   │   ├── ending_director.cpp · ending_director.h         # 五结局判定（C++ 优先级链）
│   │   ├── build_score.cpp · build_score.h                 # Build 流派评分（12 流派）
│   │   ├── build_tag.h                    # BuildTag 19 标签定义
│   │   ├── config.cpp · config.h          # 全局配置
│   │   ├── camera_director.h              # 镜头常量（HITSTOP/SHAKE/INTRO_FREEZE）
│   │   └── combat_feel.h                  # 打击感常量（Freeze/Shake 档位）
│   ├── scenes/                            # 7 场景（14）
│   │   ├── title_scene.cpp · title_scene.h               # 标题画面（粒子背景 + 菜单 N·C·F·T）
│   │   ├── game_scene.cpp · game_scene.h                 # 主场景：组合 5 Director + 帧循环（2000+ 行）
│   │   ├── floor_select_scene.cpp · floor_select_scene.h # 选关界面（F）
│   │   ├── tutorial_scene.cpp · tutorial_scene.h         # 教程（7 阶段 + P 跳过）
│   │   ├── death_scene.cpp · death_scene.h               # 死亡结算
│   │   ├── victory_scene.cpp · victory_scene.h           # 通关结算
│   │   └── credits_scene.cpp · credits_scene.h           # 片尾
│   ├── scene/                             # GameScene 拆分（6）
│   │   ├── game_scene_input.cpp · game_scene_input.h     # 输入路由（键盘/事件/圣物 R 面板）
│   │   ├── game_scene_combat.cpp · game_scene_combat.h   # 战斗结算拆分
│   │   └── game_scene_interaction.cpp · game_scene_interaction.h  # 交互/拾取/特殊房间
│   ├── director/                          # 5 Director（10）
│   │   ├── boss_system_director.cpp · boss_system_director.h     # Boss 12 子系统编排 + F10 领域 + F15 镜像
│   │   ├── gameplay_system_director.cpp · gameplay_system_director.h  # world_state/story/rels/quest/ending/run_stats
│   │   ├── presentation_system_director.cpp · presentation_system_director.h  # shake/freeze/伤害数字/BuildTheme/intro
│   │   ├── game_flow_director.cpp · game_flow_director.h  # 12 态生命周期状态机
│   │   └── mirror_combat_director.cpp · mirror_combat_director.h  # F15 镜像战斗仲裁
│   ├── entities/                          # 运行时实体（18）
│   │   ├── entity.cpp · entity.h          # 实体基类（rect/pos/speed/direction）
│   │   ├── player.cpp · player.h          # 玩家组合体（Entity+CombatStats+Inventory+Skill+Element）
│   │   ├── monster.cpp · monster.h        # 怪物组合体（+ instance_id/TeamRole）
│   │   ├── boss.cpp · boss.h              # Boss（BossAI 12 态 + 8 技能 + 连招队列 + 工厂）
│   │   ├── item.cpp · item.h              # 物品（装备/消耗品/护符 + 工厂）
│   │   ├── skill.cpp · skill.h            # 技能基类（execute + 冷却 + 进化）
│   │   ├── inventory.cpp · inventory.h    # 背包（装备/使用/丢弃）
│   │   ├── combat_stats.cpp · combat_stats.h  # 属性/伤害计算
│   │   └── ai.cpp · ai.h                  # MonsterAI 状态机（IDLE/CHASE/ATTACK + 9 原型）
│   ├── systems/                           # 无状态系统（35）
│   │   ├── combat_system.cpp · combat_system.h         # 伤害结算 + CountingRng
│   │   ├── combat_coordinator.cpp · combat_coordinator.h  # 连击系统
│   │   ├── hit_detection.cpp · hit_detection.h         # 命中判定（5 种形状）
│   │   ├── weapon_executor.cpp · weapon_executor.h     # 5 武器 × 3 段连击执行
│   │   ├── weapon_component.cpp · weapon_component.h   # 武器组件
│   │   ├── projectile_factory.cpp · projectile_factory.h  # 投射物工厂（连弩/弹幕）
│   │   ├── vfx_server.cpp · vfx_server.h               # VFX 10 基础图元 + recipe 派发
│   │   ├── game_renderer.cpp · game_renderer.h         # HUD/面板/物品栏渲染
│   │   ├── floor_manager.cpp · floor_manager.h         # 楼层推进（清怪→下楼→Boss）
│   │   ├── team_coordinator.cpp · team_coordinator.h   # 怪物队伍协同
│   │   ├── attack_evolution.cpp · attack_evolution.h   # 普攻进化管理器（Lv1→Lv3）
│   │   ├── attack_evolution_state.h                     # 普攻进化状态
│   │   ├── skill_evolution.cpp · skill_evolution.h     # 技能进化（使用次数驱动）
│   │   ├── interaction_handler.cpp · interaction_handler.h  # 拾取/交互结果解析
│   │   ├── relic_effect_processor.cpp · relic_effect_processor.h  # 圣物效果处理器
│   │   ├── reward_manager.cpp · reward_manager.h       # 奖励管理（掉落表/商店价）
│   │   ├── collision_utils.h                           # 碰撞工具函数
│   │   ├── damage_context.h                            # 伤害上下文数据结构
│   │   ├── relic_effect.h                              # 圣物效果定义
│   │   └── relic_effect_runtime.h                      # 圣物效果运行时状态
│   ├── world/                             # 世界生成与规则（38）
│   │   ├── dungeon_generator.cpp · dungeon_generator.h # BSP 二分划分地图生成（Phase 2: Door 拓扑）
│   │   ├── game_map.cpp · game_map.h     # 瓦片地图（碰撞/特殊房/熔岩/木桶/FOV 可见性）
│   │   ├── biome.cpp · biome.h           # 三章生态（Prison/Volcano/Abyss）
│   │   ├── landmark.cpp · landmark.h     # 地标（9 个）
│   │   ├── encounter.cpp · encounter.h   # 遭遇框架（9 个：NPC/事件）
│   │   ├── special_room.cpp · special_room.h           # 11 类特殊房间
│   │   ├── challenge_room.cpp · challenge_room.h       # 挑战房（7 阶段状态机 + 波次战斗）
│   │   ├── room_manager.cpp · room_manager.h           # 房间管理（布局/邻接/门）
│   │   ├── quest_manager.cpp · quest_manager.h         # 任务管理（12 任务）
│   │   ├── npc_system.cpp · npc_system.h               # NPC 系统（12 NPC 类型）
│   │   ├── relationship_system.cpp · relationship_system.h  # 好感度
│   │   ├── rule_chain.cpp · rule_chain.h               # Boss 死亡 → 规则链激活
│   │   ├── event_system.cpp · event_system.h           # 世界事件（18 类型）
│   │   ├── world_state.cpp · world_state.h             # Flags + Counters
│   │   ├── flow_director.cpp · flow_director.h         # 动态内容编排
│   │   ├── floor_config.cpp · floor_config.h           # 楼层配置（倍率/特殊房/BGM）
│   │   ├── floor_narrative.cpp · floor_narrative.h     # 15 层叙事
│   │   ├── growth_curve.cpp · growth_curve.h           # 难度曲线
│   │   └── world_reaction.cpp · world_reaction.h       # 全局色调反应
│   ├── boss/                              # Boss 子系统（18）
│   │   ├── boss_behavior.cpp · boss_behavior.h         # 决策/人格/记忆
│   │   ├── boss_evolution.cpp · boss_evolution.h       # 技能变体 / LastStand
│   │   ├── boss_narrative.cpp · boss_narrative.h       # 自适应对话查询
│   │   ├── boss_cinematic.cpp · boss_cinematic.h       # intro/phase2/death 演出
│   │   ├── boss_encounter.cpp · boss_encounter.h       # 阶段控制（遭遇→连招模板）
│   │   ├── boss_replay.cpp · boss_replay.h             # 战斗记忆（学习/评估）
│   │   ├── boss_timeline.cpp · boss_timeline.h         # 战斗时间线
│   │   ├── boss_command.cpp · boss_command.h           # 决策 → 命令执行（含冷却判定）
│   │   └── arena_manager.cpp · arena_manager.h         # 领域地形（DangerZone/熔岩环带）
│   ├── combat/                            # 元素战斗（2）
│   │   └── element_resolver.cpp · element_resolver.h   # 元素伤害结算（火暴击/冰冻结/毒 DOT）
│   ├── components/                        # 组件（2）
│   │   └── element_component.cpp · element_component.h # 元素等级/经验
│   ├── core/                              # 引擎核心（4）
│   │   ├── event_bus.cpp · event_bus.h    # 事件总线（45 事件）
│   │   ├── event_types.h                  # 事件枚举 + GameEvent 载荷
│   │   └── service_locator.h              # 服务定位
│   ├── rendering/                         # 渲染（4）
│   │   ├── sprite_renderer.cpp · sprite_renderer.h     # sprite atlas + 程序化像素占位
│   │   └── door_renderer.cpp · door_renderer.h         # 门动画渲染（Phase 2 Door 拓扑）
│   ├── ui/                                # UI 渲染（Phase 3 Minimap）
│   │   └── minimap.cpp · minimap.h        # 小地图（只读 isExplored/isVisible，无第二套状态）
│   ├── resources/                         # 资源管理（2）
│   │   └── resource_manager.cpp · resource_manager.h   # 字体/纹理/JSON 缓存
│   ├── audio/                             # 音频（6）
│   │   ├── wave_synth.cpp · wave_synth.h  # 程序化波形合成（零素材）
│   │   ├── bgm_engine.cpp · bgm_engine.h  # 5 BGM 编译 + 交叉淡入 + Phase2 cue
│   │   └── audio_server.cpp · audio_server.h           # SFX 播放/音量/静音
│   ├── save/                              # 存档（2）
│   │   └── save_manager.cpp · save_manager.h           # v3 key:value 存档（v1→v3 兼容）
│   ├── ai/player_behavior/                # 玩家行为采集（8）
│   │   ├── player_action.h                # 玩家动作定义
│   │   ├── player_behavior_recorder.cpp · player_behavior_recorder.h  # 行为采集（镜像数据源）
│   │   ├── player_behavior_analyzer.cpp · player_behavior_analyzer.h  # 行为分析（统计特征）
│   │   ├── player_habit_profile.cpp · player_habit_profile.h          # 习惯画像（反击型/单向癖等）
│   │   └── player_behavior_data.h         # 行为数据结构
│   ├── types/                             # 共享类型（6）
│   │   ├── combat_types.h                 # 战斗类型
│   │   ├── boss_types.h                   # Boss 类型
│   │   ├── weapon_types.h                 # 武器类型
│   │   ├── meta_types.h                   # 局外成长类型
│   │   ├── story_types.h                  # 剧情类型
│   │   └── world_types.h                  # 世界类型
│   └── tutorial/                          # 教程（2）
│       └── tutorial_guide.cpp · tutorial_guide.h       # 7 阶段教程
│
├── ai/                                   # AI 研究层（43）
│   ├── agents/                            # 决策 Agent（3）
│   │   ├── ai_agent.h                     # Agent 接口
│   │   └── bt_agent.cpp · bt_agent.h      # 行为树驱动玩家（--sim-ai bt）
│   ├── behavior_tree/                     # 行为树节点库（8）
│   │   ├── bt_node.h                      # 节点基类
│   │   ├── bt_selector.h · bt_sequence.h  # 组合节点（选择/序列）
│   │   ├── bt_condition.h                 # 条件节点
│   │   ├── bt_action.h                    # 动作节点
│   │   ├── bt_move_to_target.h            # A* 移动节点
│   │   ├── behavior_tree.h                # 树容器
│   │   └── blackboard.h                   # 黑板（共享状态）
│   ├── mcts/                              # 蒙特卡洛树搜索（8）
│   │   ├── mcts_node.h                    # UCT 节点
│   │   ├── mcts_search.cpp · mcts_search.h            # UCT 搜索主循环
│   │   ├── simulation_state.cpp · simulation_state.h  # 对局深克隆模拟
│   │   ├── combat_evaluator.cpp · combat_evaluator.h  # 终局评估（±1000）
│   │   └── action.h                       # 动作空间
│   ├── rl/                                # 强化学习（9）
│   │   ├── environment.cpp · environment.h            # Gym-like 环境（reset/step/reward/done）
│   │   ├── observation.cpp · observation.h            # 观测向量 + 离散化键
│   │   ├── q_agent.cpp · q_agent.h        # Q 表 Agent（epsilon 退火 + 学习率衰减）
│   │   ├── random_agent.cpp · random_agent.h          # 随机基线
│   │   └── rl_runner.cpp                  # 训练主循环（--rl-train/--rl-mirror）
│   ├── mirror/                            # 镜像学习 Boss（13）
│   │   ├── mirror_agent.cpp · mirror_agent.h          # 三阶段人格（观察/镜像/进化）
│   │   ├── behavior_clone_table.cpp · behavior_clone_table.h  # 行为克隆（状态桶 d:h:s）
│   │   ├── tactical_chain_table.cpp · tactical_chain_table.h  # 11 符号 n-gram 战术链
│   │   ├── online_adaptive_policy.cpp · online_adaptive_policy.h  # Thompson 采样（Beta 后验）
│   │   ├── rolling_accuracy.cpp · rolling_accuracy.h  # 滚动准确率
│   │   ├── mirror_tuning.h                # 阶段进入参数（阈值/兜底）
│   │   └── mirror_debug_stats.cpp · mirror_debug_stats.h  # 仲裁分布统计
│   └── navigation/                        # 导航（2）
│       └── pathfinder.cpp · pathfinder.h  # A* 寻路（priority_queue + Manhattan）

tests/                      (53 条目)  # GoogleTest，按模块分目录 — fov / dungeon_topology / minimap / economy 等
resources/                             # 20+ JSON 配置（enemies/skills/relics/bosses/weapons/elements/biomes/encounters/dialogues/quests/endings/…）+ world/ 三章子目录
tools/                                 # world_validator.py / extract_chars.py / replace_methods.py
docs/                                  # ARCHITECTURE / G4_PLATFORM_BIBLE / WORLD_LORE / D1_GAMEPLAY / ART_*
vendor/                                # raylib 5.0（include + lib）+ nlohmann/json（header-only）
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
| `docs/AI_LEARNING_GUIDE.md` | AI 子系统源码级审查（11 处修复 + 剩余优化路线图） |

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
| G1.6 | BossDef 数据模块 + bosses.json (5 bosses 数据驱动) + Phase2 参数化 + Vampire 新Boss | ✅ |
| G1.7 | Save v2: atl + skill evo/use + rule_counters 序列化 + 向后兼容旧存档 | ✅ |
| G2.0 | Infrastructure Polish: 4 Def 统一接口 (get_all + is_loaded) + 重复 ID 检测 + 加载日志标准化 | ✅ |
| G2.1 | Dialogue Data Driven: dialogues.json + DialogueDef + BossNarrative 重构 | ✅ |
| G2.2 | TeamAI: TeamCoordinator + TeamDecision + MonsterAI 重构 (143→55 行) | ✅ |
| G2.3 | Boss Arena v2: BossArenaDef + ArenaEvent + execute_event() | ✅ |
| G2.4 | QuestDef + quests.json (12 quests) + EventBus quest events + Save v3 qst: | ✅ |
| G2.5 | EndingDef + endings.json + Save v3 end: + ENDING_REACHED emit | ✅ |
| G3.1 | MetaNodeDef + meta_nodes.json + MetaProgression::load_from_defs() (10 nodes) | ✅ |
| G3.2 | SkillDef + skills.json (22 skills) + SkillFactory + _skill_id 替代 dynamic_cast | ✅ |
| G3.3 | ItemDef + items.json (31 templates) + ItemFactory 替代硬编码数组 | ✅ |
| G3.4 | Architecture Freeze: 命名统一 + 12 模块 API 审计 | ✅ |
| G3.5 | Meta Reward Integration: reward_from_ending() + MetaRewardRecord 审计日志 | ✅ |
| G4.1 | Mod Loader: IRegistryProvider + RegistryBuilder + BuiltinProvider + ModProvider + 12×_from_json | ✅ |
| G4.1.5 | Registry Validator: cross-ref checks (Skill→Buff, Item→Skill, Enemy→Buff) + required fields | ✅ |
| G4.2 | Namespace ID (mod_id:entry_id) + DependencyResolver + Merge v2 (topological sort) | ✅ |
| G4.3 | Advanced Merge: MergePatch (__patch field merge) + merge_patch.h helper + BuildRecord patch | ✅ |
| G4.4 | ModManager: scan/enable/disable/list + mods/config.json + startup summary | ✅ |
| G4.5 | Replay Regression: ReplayFile + Recorder + Player + _is_action + state_hash + seed_rng + CLI | ✅ |
| G5.1 | Build Diversity: BuildType 6→12 + skills 6→22 + relics 33→60 + buffs 20→27 + items 20→31 + enemies 10→30 | ✅ |
| G5.2 | Signature Skills: IceNova/ChainLightning/ShadowStrike/BloodFrenzy/SummonSpirit | ✅ |
| G5.3 | Enemy Archetypes: AIArchetype + SNIPER/CONTROLLER/AMBUSH/GUARDIAN + enemies 23→30 | ✅ |
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
| G9.0 | Weapon Framework: WeaponType(5)/HitShape(5) + WeaponDef registry + weapons.json (21 entries) | ✅ |
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
| Phase 0 | 可见性/拓扑/数据一致性审计 + 平衡审计（docs/VISIBILITY_SYSTEM_AUDIT / DUNGEON_TOPOLOGY_AUDIT / P0_5_BALANCE_AUDIT） | ✅ |
| Phase 1 | FOV 可见性系统（360° 射线投射）：Tile 三层状态（未探索/可见/记忆暗）+ 实体中心 tile 剔除 + 8 单测；阻断可见性 bug | ✅ |
| Phase 2 | 地牢拓扑：Room→Door→Corridor 边缘连接（_pick_room_edge + _compute_door_pos）+ DOOR tile（walkable、不挡视线）+ _carve_diamond 墙壁保护 + 12 拓扑测试 + 5 结构回归 | ✅ |
| Phase 3 | Minimap 小地图：MinimapRenderer 只读 isExplored/isVisible（无第二套状态）+ M 键开关右下角面板 + Boss 最后已知位置/楼梯发现地标 + 12 单测 | ✅ |
| v1.2.3 | 怪物房间边界约束（AI 视觉/移动/技能/脱困全部限制在出生房间内）+ 小地图上移避让快捷键文字; 42/42 测试 | ✅ |
| v1.2.5 | 混合码位修复: 重生成字体码位表 (+35字) + 对话箭头 ▶→→ | ✅ |
| v1.2.6 | 特殊房间装饰素材上线 (9素材) + 装甲/药水/护符图标 | ✅ |
| Batch 3A | 经济基础: PersistenceScope(FLOOR/RUN) + Player 金币/钥匙 + get_sell_value/sell_item + RewardManager + Save v4 + HUD 金币钥匙显示 + 5 测试 | ✅ |
| Batch 3B | 赌徒房 MVP: 金币开房(40+floor×10) + 75/20/5 奖励池 + RUN 圣物 + 耗尽→钥匙回退 + is_repeatable 旁路 + 8 测试 | ✅ |
| Batch 3C | 背包出售 UI: [T] 出售 + sell_selected_item 静态 helper + key hints 更新 + 8 测试 | ✅ |
| Batch 3D | 挑战房架构审计: SpecialRoom/Key/Room/Monster/Reward 全系统审查 + 最终实现计划 | ✅ |
| Batch 3E | 挑战房设计冻结: 7阶段状态机 + ChallengeRoomController + 3×4波次 + 确定性RNG + 奖励背包满fallback | ✅ |
| Batch 3F | 挑战房 MVP: ChallengeRoomController + 7-phase state machine + 出口附近放置 + 波次战斗 + HUD wave 显示 + 14 测试 | ✅ |
| Batch 3G | 挑战房 HUD: 进度条 + 击杀计数 + 剩余波次 + floor 横幅 + Boss 变体 (精英/双怪) + 10 测试 | ✅ |
| Batch 3H | 赌徒房 UI 重做 + 挑战房完整 MVP: 传送门系统 + E键交互 + 选择面板 + 返回传送门 + 独立竞技场地图 + 53/53 测试 + 桌面同步 | ✅ |