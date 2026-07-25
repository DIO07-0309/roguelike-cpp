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

## 敌人

31 种敌人，9 类 AI。Boss 有专属技能 + 狂暴机制。

| 楼层 | 主题 | 池 |
|------|------|-----|
| F1–5 | 遗忘监狱 | 骷髅弓手/骨兵/史莱姆/暗影行者 |
| F6–10 | 灰烬火山 | 火妖/精英兽人/冲锋兽人 |
| F11–15 | 虚空深渊 | 暗术师/虚空行者/石像守卫 |

**三场 Boss 战**：暗影骑士(F5) / 地狱火魔(F10) / 深渊之主(F15)

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

## 开发记录

| Milestone | 状态 |
|-----------|------|
| M1–M30 | 游戏全部系统（Boss/Buff/房间/圣物/NPC/任务/事件/结局） | ✅ |
| G1–G4 | 数据驱动重构、Mod 系统、存档升级、Replay | ✅ |
| G5 | 技能扩展 (20 skill)、表现层 (VFX/Audio/Camera) | ✅ |
| G6 | 世界层 (Biome/Landmark/Hazard/Encounter) | ✅ |
| G7 | 工程质量 (Validator/Test/Simulation/ModSDK) | ✅ |
| G8 | 智能 AI (BT/Navigation/MCTS/RL) | ✅ |
| **G9** | **武器系统重写 (6武器×3段连击/命中判定/弹道/协同)** | ✅ |
