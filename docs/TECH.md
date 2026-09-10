# 回响深渊 — 技术文档（开发者版）

> 本文档承接 README 的技术细节，面向贡献者/研究者。玩家向内容见 `README.md`。
> 架构唯一权威：`docs/ARCHITECTURE.md`。

## 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17（`unique_ptr` / `shared_ptr` / `std::optional` / `enum class`），组合优于继承，函数 ≤40 行规范 |
| 图形/输入 | Raylib 5.0（窗口/绘制/输入/音频），sprite atlas + 程序化像素占位 |
| JSON | nlohmann/json（header-only），20+ 配置文件全数据驱动 |
| 构建 | CMake 3.16+ + CMakePresets + MinGW（UTF-8 编译标志），Release/Debug + 测试三配置 |
| 测试 | GoogleTest（60 ctest 条目）+ GitHub Actions CI + `world_validator.py` 数据校验 |
| Python 工具 | `tools/`：world_validator（JSON 交叉引用）/ extract_chars（中文字体码点）/ m5_sprite_gen（像素图生成） |

## 工程与架构技术

- **数据驱动 + Def/Runtime 分离** — JSON 不可变配置 ≠ C++ 可变状态；Registry 只读查询（`load_/get_/get_all_/is_loaded` 统一 API）；20+ JSON → 12 模块加载器 → 运行时
- **Mod 热插拔** — `IRegistryProvider` 优先级链 + MergeMode{Skip/Replace/**MergePatch** 字段级合并} + 依赖**拓扑排序** + 循环检测 + `mod_id:entry_id` 命名空间隔离
- **事件驱动解耦** — EventBus 45 事件、轻量载荷 `{type,sender,int,float,str}`、按 owner 批量注销；Gameplay→EventBus→Presentation 单向流（Gameplay 不引用 UI）
- **组合式 Director** — GameScene 组合 5 Director：BossSystem（12 子系统）/ GameplaySystem（world_state/quest/ending）/ PresentationSystem（shake/freeze/BuildTheme）/ GameFlow（12 态生命周期）/ Flow（动态内容编排）；CameraDirector 常量 + EndingDirector 判定为辅助模块；零继承
- **确定性游戏技术** — `CountingRng`（mt19937 + 掷骰计数）· 种子公式 `seed_start + run*1234567` · **replay hash 链**逐帧校验（mixer 黄金比例常量）· 指针键 → instance_id 防跨进程分叉（Q3.14 对拍逐字节一致）
- **双 RNG 流隔离（G9.3/RNG-001）** — gameplay `rng` 与视觉 `visual_rng` 严格分离；渲染层差异（素材命中与否）不得污染 gameplay 随机流
- **存档兼容工程** — v1→v5 追加式字段 + `getV` 默认值 + 旧技能名映射 + SaveStable 验收测试；四份数据独立：slot_N.json（局内×3槽）/ meta_save.json（局外成长）/ relic_archive.json（收藏）
- **内存安全实践** — 全智能指针 + 工厂方法（`spawn_monster`/`boss_factory_create`），无裸 `new`；SEH 异常捕获 → crash.log
- **中文字体管线** — NotoSansCJKsc-Regular.otf（思源黑体，OFL-1.1）+ `extract_chars.py` 精确码位扫描（1831 码点）→ `GuiFont::DrawTextCH()`（Raylib DrawText 不支持中文）

## 算法与 AI 技术

- **BSP 二分划分** 随机地牢 + Seed 驱动确定性（同种子同地图，跨进程可对拍）
- **行为树** — Selector/Sequence/条件/动作 + 黑板（BTAgent 根 Selector 8 节点优先序）
- **MCTS** — UCT 搜索（C=1.414，100 迭代，深度 10，奖励 sigmoid 归一化）+ SimulationState 深克隆
- **Q-Learning** — 观测离散化 Q 表 + epsilon 退火 0.12→0.005 + 学习率按访问衰减 + 终局 done 处理 + RL 自博弈（95%+ 收敛）
- **n-gram 序列建模** — 11 符号 3-gram 计数表（键 `s0*121+s1*11+s2`）+ 2-gram 分母降级链
- **Thompson 采样** — Beta 后验多臂赌博机在线学习（命中奖励持续更新 + 全臂折扣遗忘防先验爆炸）
- **状态桶离散化** — `d<距离>:h<血量>:s<技能>` 三维聚类 → 行为克隆预测（exact→fuzzy→profile→default 降级）
- **A\***（priority_queue + Manhattan 启发式）生产测试双用 + **BFS** 危险避让（熔岩/毒池/尖刺/木桶）
- **Headless 确定性模拟器** — 定步长 1/60 批量评估（500 局 53s，9.4 局/s/核），胜率/死亡分布/Build 评级自动报告

## AI 架构

### 玩家侧决策（模拟器驱动）

| 系统 | 实现 | 细节 |
|------|------|------|
| **DecisionAgent** | 评分式决策（`src/core/sim/sim_ai.cpp`） | attack/skill/move/pickup/heal 五类计分取最大；BuildType 12 流派感知；`--sim-ai decision`（默认） |
| **BTAgent** | 行为树（`src/ai/agents/bt_agent.cpp`） | 根 Selector 8 子节点优先序 — BossIntro→确认 / Stairs→下楼 / 低血→自愈 / BossNear→攻击 / AoE / EnemyNear / 拾取 / Wander 兜底；16 测试 |
| **MCTS** | UCT 搜索（`src/ai/mcts/`） | C=1.414，100 迭代，深度上限 10，奖励 sigmoid 归一化，终局 ±1000；16 测试；`--sim-ai mcts` |
| **Q-Learning** | RL 环境（`src/ai/rl/`） | Gym-like API（reset/step/reward + done）+ 观测离散化 Q 表；17 测试；`--rl-test/train` |

### MirrorAgent 详解（F15 镜像学习核心）

定位：**分析层非控制层** — 读玩家习惯调整 BossAI 参数，不直接调用 attack/move（`mirror_agent.h:21`）。三阶段人格：

| 阶段 | 进入条件（`mirror_tuning.h` 全部参数） | 行为 |
|------|------|------|
| **P1 观察** | — | 实时采集：攻击/技能 0.5s 窗口、位移、喝药识别；Boss 复制玩家武器/技能 |
| **P2 镜像** | 观察 ≥20 次 或 40 次兜底 / 时间 12s 兜底 / 准确率 ≥0.65 | 克隆预测（BehaviorCloneTable）+ 战术链 n-gram 反制 |
| **P3 进化** | 同桶命中 10 次 / 准确率 ≥0.70 / 玩家 HP <0.35 危险线 | 在线学习：reward=命中，Thompson 多臂持续探索 |

- **决策接口**：`recommend_action`（Thompson 采样，phase<2 返回 -1）/ `report_outcome`（Beta 后验更新）/ `should_interrupt_skill` / `should_pressure_close` / `predict_next_action` / `recommend_distance`
- **跨局记忆**：alpha+beta 各 36 float（9 桶 × 4 臂）→ 存档 `mra:`/`mrb:` → 新局旧后验叠加为先验（`inject_mirror_memory`）；导出时扣除本局画像先验 + 全臂折扣遗忘 λ=0.995
- **漂移自适应**：策略漂移 >0.5 时克隆置信门槛 0.50→0.75
- **战斗快照** MirrorBattleState：boss_hp_pct / player_hp_pct / dist_tiles / player_attacking / player_using_skill / boss_can_attack / boss_in_domain / player_skills_ready

### 仲裁链（镜像 Boss 决策，五层）

```
ML 插槽(默认关) → 战术链(n-gram) → RL(Q 表 exploit) → 克隆(行为预测) → Thompson 采样 → 规则兜底
```

| 层 | 数据结构 | 机制 |
|------|------|------|
| ML | 预测器插槽 `set_ml_predictor` | 默认关闭，未训练 |
| 战术链 | TacticalChainTable | 11 符号 3-gram 计数表 + 2-gram 分母 + 单前缀；降级链 3-gram→2-gram→1-gram |
| RL | QAgent（4 风格 Q 表） | 离线训练 95%+ 收敛，运行时按玩家画像加载，exploit 为主 |
| 克隆 | BehaviorCloneTable | PlayerIntention 7 类；CloneContext 状态桶 `d<dist>:h<hp>:s<skills>`；降级链 exact→fuzzy→profile→default |
| Thompson | OnlineAdaptivePolicy | Beta 后验采样 + 全臂折扣遗忘 λ=0.995 |

实测仲裁分布（v0.9.30，500 局）：`[Clone:0 ML:0 RL:11/25/26 Tho:0]` — RL 完全接管。

### RL 训练管线

- `--rl-train N`：通用 Q 表（`saves/rl_qtable.json`，~2380 条目）
- `--rl-mirror N`：镜像 Boss 自博弈，4 风格 Q 表（`saves/rl_mirror_q_<STYLE>.json`）
- epsilon 退火 0.12→0.005，末段 10% 低探索统计收敛：AGGRESSIVE 96.8% / DEFENSIVE 99.0% / SNIPER 96.4% / BALANCED 99.2% / TRAIN 100%
- 运行时按玩家画像风格加载（缺失安全降级跳过）

### 导航与模拟

- 模拟器用 **BFS**（`_bfs_toward/_bfs_away`，tile 级 rect 碰撞 + 危险避让）
- **A\*** pathfinder 供 BT MoveToTarget 节点
- `--sim N --sim-seed S`：headless 定步长 1/60，胜率 / 平均楼层 / Boss 击杀率 / Build 评级 / 圣物 TOP10 / 威胁度

## F15 Mirror Boss — 终焉回响

### 镜像机制

| 项 | 实现 |
|------|------|
| **武器同步** | 复制玩家武器 3 段连击（近战=玩家武器 / 远程=CROSSBOW×0.8） |
| **技能同步** | 按玩家主动技能逐槽镜像，关键词→类型映射 |
| **属性同步** | HP=玩家×2.5，ATK=玩家×0.85，防御=玩家+5/+3，自愈≈6.7% maxHP |
| **真冻结** | Phase≥2 冻结玩家 1.5s（禁移动/攻击），镜像仍可行动 |
| **节奏** | 决策间隔 0.5s，行为状态机 approach/attack/skill/retreat，追击 120px/s |

### 三阶段玩家感受

Phase 1 *我在打自己* → Phase 2 *它在预测我* → Phase 3 *它比我更懂我*

## 验证数据

### 平衡回归（500 局 / 5 seeds，目标区间 6-10%）

| 版本 | 胜率 | 备注 |
|------|------|------|
| Q3.12 基线 | **5.8%** | 死亡分布 F1-5≈40% F6-10≈50% F11-15≈8% |
| v0.9.30 | **6.6%** | RL 决策层接入镜像 |
| v0.9.31 | **7.0%** | M4b 地狱火魔领域作战 |
| v0.9.32 | **8.0%** | 五项 Stable 验收 |
| v0.9.33 | **10.0%** | 收官体检 |
| v0.9.34 | **9.0%** | AI 审查修复 11 处算法 bug |

### 确定性

- Q3.14 对拍：3 种子 × 20 局 × 2 批逐字节一致（130 万行级）
- 修复三类跨进程分叉：指针键 → instance_id/uint64
- Replay hash 链（`compute_state_hash` 逐帧链式 + `verify_hash_chain`）
- **已知边界（P1-C8 候选）**：v1.4.13 后观察到同 exe 同种子 12 连跑 3 次间歇分岔（F3 毒 tick 时序差 1 条日志），检出概率性，根因待查 — 不影响发布（玩家不可感知），影响 sim 研究基线精度

### 性能

- sim 500 局（5 进程并行）：53s，单核 ~9.4 局/s
- 全量测试套件：<1s

## 命令行参数

| 参数 | 说明 |
|------|------|
| `--sim N` | 跑 N 局模拟（`--sim-seed S` 种子） |
| `--sim-ai bt/mcts/decision` | 模拟 AI 类型 |
| `--rl-train N` | Q-Learning 训练 |
| `--rl-mirror N` | 镜像 Boss 自博弈训练 |
| `--record <path>` / `--replay <path>` | 回放录制/播放 |
| `--sim-build` / `--sim-all-builds` | Build 流派评估 |

## 设计文档索引

| 文档 | 内容 |
|------|------|
| `docs/ARCHITECTURE.md` | 模块架构 |
| `docs/WORLD_LORE.md` | 世界观设定 |
| `docs/D1_GAMEPLAY_LOOP_DESIGN.md` | 战斗循环设计 |
| `docs/G4_PLATFORM_BIBLE.md` | 平台兼容 + Release Standard |
| `docs/V1_0_0_ACCEPTANCE.md` | 五项 Stable 验收报告 |
| `docs/AI_LEARNING_GUIDE.md` | AI 子系统源码级审查 |
| `docs/RELEASE_CHECKLIST.md` | v1.5.0 Demo 发布门禁 |

## 开发进度索引

完整开发流水（M1→M20 / D1-D6 / G1-G10 / Phase 0-3 / Batch 3A-3I / P1 系列）见 `CHANGELOG.md` 按版本倒序浏览；里程碑总表见 git log。此处不再维护第二份流水账（避免双源失同步）。
