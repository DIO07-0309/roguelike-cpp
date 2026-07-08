# Roguelike — 地牢肉鸽 (C++ Raylib 版)

> 重庆大学大数据与软件学院 ·《程序设计实训》  
> 开发者：ruozhiDIO

---

## 一、游戏简介

你是一名冒险者，进入一座由程序随机生成的地牢。每局地图不同，每次死亡都是永久性的。

**目标**：击败最深处的 Boss「深渊之主·终焉」。

**引擎**：C++17 + Raylib 5.0  
**构建**：CMake + MSVC / MinGW  
**平台**：Windows / macOS / Linux

---

## 二、快速开始

### 编译

```bash
# 1. 安装依赖
# Raylib 5.0 放到 vendor/raylib/ (include/ + lib/)
# nlohmann/json 头文件放到 vendor/json/include/ (或不需要，本版纯C标准库)

# 2. CMake 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. 运行
build/roguelike_cpp.exe
```

### 桌面版（免编译）

直接双击 `Roguelike-CPP版/roguelike_cpp.exe` 即可运行（约 2 MB，含 raylib.dll）。

---

## 三、操作一览

### 移动与交互

| 按键 | 功能 |
|------|------|
| **W A S D** | 上下左右移动 |
| **↑ ↓ ← →** | 切换朝向（不移动） |
| **空格** | 普攻攻击最近的怪物 |
| **E** | 拾取地面物品 / 触发特殊房间 |
| **I** | 打开 / 关闭背包 |
| **>** | 下楼（需楼梯激活） |

### 技能释放

| 按键 | 功能 |
|------|------|
| **1** | 释放第 1 个主动技能 |
| **2** | 释放第 2 个主动技能 |
| **3** | 释放第 3 个主动技能 |
| **4** | 释放第 4 个主动技能 |

### 背包操作

按 **I** 打开背包后：

| 按键 | 功能 |
|------|------|
| **↑ / ↓** | 移动光标选择物品 |
| **X** | 装备选中物品（武器/护甲自动替换旧装备） |
| **U** | 使用选中物品（药水回血） |
| **D** | 丢弃选中物品 |
| **I / Esc** | 关闭背包 |

### 系统操作

| 按键 | 功能 |
|------|------|
| **N** | 新游戏 |
| **C** | 继续游戏（需有存档） |
| **F** | 选关 |
| **T** | 新手教程 |
| **Enter** | 确认 / 开始 |
| **Esc** | 游戏中返回标题 |
| **Esc** (标题) | 退出游戏 |
| **F11** | 全屏切换 |
| **P** (教程) | 跳过当前步骤 |

---

## 四、新手教程

标题画面按 **T** 进入教程，逐步学习所有操作：

| 步骤 | 内容 | 过关条件 | 按P跳过 |
|------|------|----------|---------|
| 1 | 欢迎 | Enter 开始 | — |
| 2 | **WASD 移动** | 走 4 格以上 | ✅ |
| 3 | **空格攻击** | 打木桩 1 次 | ✅ |
| 4 | **E 拾取** | 捡起地面药水 | ✅ |
| 5 | **I 背包 + U 使用** | 开背包用掉药水回血 | ✅ |
| 6 | **X 装备武器** | 在背包内装备短剑 | ✅ |
| 7 | **1 释放技能** | 使用斩击技能 | ✅ |

卡关时按 **P** 直接跳过当前步骤，按 **T** 退出教程。

---

## 五、游戏系统介绍

### 5.1 战斗

- **普攻**：空格键攻击，基础伤害 = ATK − DEF×0.5，有 ±20% 随机浮动
- **冷却**：普攻 0.5 秒冷却，怪物 1.5 秒冷却
- **死亡**：HP 归零 → 死亡画面 → Enter 返回标题（存档保留）

### 5.2 装备系统（4 种稀有度）

| 稀有度 | 颜色 | 属性倍率 | 掉落率 |
|--------|------|----------|--------|
| **普通** | 灰色 | ×1.0 | 60% |
| **稀有** | 蓝色 | ×1.5 | 25% |
| **史诗** | 紫色 | ×2.0 | 12% |
| **传说** | 橙色 | ×3.0 | 3% |

- **武器（W）**：提升攻击力
- **护甲（A）**：提升防御力
- **护符（C）**：强化指定技能效果
- **药水（P）**：恢复生命值

### 5.3 技能系统

**主动技能**（按键 1-4 释放，带冷却 + 3 级升级 + 护符加成）：

| 技能 | CD | 效果 |
|------|----|------|
| **斩击** (SlashSkill) | 2s → 1.2s | 前方扇形范围伤害（ATK×1.5，Lv3 二连击） |
| **神罚** (FireballSkill) | 5s → 3s | 远程多目标魔法伤害（ATK×2.5，Lv3 三目标） |
| **自愈** (SelfHealSkill) | 8s → 5s | 恢复 HP + Lv3 持续再生 |
| **The World** (TheWorldSkill) | 20s → 12s | 时停冻结全屏怪物 5→10 秒 |

**被动技能**（习得后常驻生效）：

| 技能 | 效果 |
|------|------|
| **铁壁** (IronSkinSkill) | Lv1:+3 / Lv2:+7 / Lv3:+12 双防 |
| **狂暴** (BerserkSkill) | Lv1:+3 / Lv2:+7 / Lv3:+12 攻击 |

**升级机制**：
- 击杀怪物 → XP（Boss 150、普通 10 + HP×0.5）
- 经验公式：Level² × 20 XP
- 每升一级：HP 回满、ATK+2、DEF+1、MaxHP+10
- 主动技能未满 4 个时，自动习得不重复新技能

### 5.4 怪物 AI

```
巡逻（IDLE）── 距离 ≤ 视野 ──→ 追击（CHASE）── 距离 ≤ 攻击范围 ──→ 攻击（ATTACK）
    ↑                                                                   │
    └──────────── 脱离视野 ──────────────────────────────────────────────┘
```

- **视野**：6 格（Boss 10 格）
- **巡逻**：每 2 秒随机八方向换向，30% 停顿
- **追击**：直线向玩家移动，速度 80 px/s
- **攻击**：1.5 秒冷却，站定攻击

### 5.5 Buff 状态系统（B1~B7 完整实现）

**Buff 类型**（3 种，配置化 JSON 驱动）：

| Buff | 类型 | 效果 | 持续 | 最大层数 | 来源 |
|------|------|------|------|----------|------|
| **中毒** (poison) | DOT | 每层每 0.5s 跳 3 点物理伤害 | 4s | 5 | 斩击、兽人攻击 |
| **减速** (slow) | STAT_MOD | 每层移速 ×0.7 | 3s | 3 | 神罚、史莱姆攻击 |
| **攻击提升** (attack_up) | STAT_MOD | 每层攻击力 +30% | 6s | 3 | 自愈、力量药剂 |

**玩法接入**（B6）：
- **技能**：斩击 30% 上毒、神罚 25% 减速、自愈必定攻击提升
- **怪物**：史莱姆 25% 减速、兽人 25% 上毒
- **道具**：力量药剂（20% 掉落概率）使用后攻击提升 6s

**触发规则统一化**（B7）：
- 所有 Buff 触发规则统一为 `std::vector<BuffTrigger>`（buff_id + stacks + chance + target）
- 挂载在 `ActiveSkill.triggers` / `Monster.on_hit_triggers` / `ConsumableItem.triggers`
- 提供 `apply_triggers()` / `apply_triggers_self()` 两个统一 helper

**Buff HUD**（B3）：
- 玩家 HUD：技能栏下方显示"显示名 ×层数 剩余时间"
- 怪物标签：头顶简写标签（毒×2 / 慢×1 / 攻×2）

**配置化**（B4）：`resources/buffs.json` — 加载到 `g_buff_defs` map，display_name/short_name/hud_color 均来自 JSON。自定义精简 JSON 解析器（零第三方依赖）。

**存档恢复**（B5）：`buf:` 行完整存储 id/stacks/remaining/tick_timer，老存档无 `buf:` 字段自动兼容。

**生命周期**（B2）：
- 死亡对象不再 tick buff
- poison 毒死怪物 → 复用 `_on_monster_killed()` → XP/掉落/楼梯清怪
- DOT 用 `while` 处理大 dt 跨多个 tick_interval，不丢 tick
- 同 id buff 叠层刷新持续时间，POISON 叠层不重置 tick_timer

### 5.6 Boss

三场 Boss 战（第 5 / 10 / 15 层）：

| Boss | 层 | HP | ATK | 背景 |
|------|----|----|-----|------|
| **暗影骑士** | 5F | 250 | 15 | 曾是王城最荣耀的骑士，被黑暗吞噬 |
| **地狱火魔** | 10F | 500 | 24 | 熔岩深渊中诞生的远古恶魔 |
| **深渊之主·终焉** | 15F | 900 | 35 | 地牢最深处的终极存在 |

**Boss 专属技能**：

| 技能 | CD | 效果 |
|------|----|------|
| **暗影冲击** | 5s | 前方锥形范围物理伤害（ATK×1.8） |
| **地裂** | 7s | 周围圆形 AOE 魔法伤害（ATK×1.2） |
| **召唤兽人** | 15s | 召唤 2 只兽人小怪 |

**狂暴**（HP ≤ 40%）：移速 ×1.6，技能 CD×0.7，永久。

**击杀奖励**：
- 必掉「魔渊之刃」（传说武器）
- 必掉「神谕药剂」（传说药水）
- 自动学习新技能

### 5.6 楼层系统

- **15 层关卡**，每 5 层一个 Boss
- 清空当前层所有怪物 → 金色楼梯激活 → 按 > 下楼
- 清空楼层时自动存档
- **难度递增**：怪物 HP×1.0→3.3，ATK×1.0→2.9

### 5.7 特殊房间系统（B8/B9/B10 完整实现）

每层地牢中随机分布 2~3 个特殊房间，房间类型通过 `SpecialRoomType` 强类型枚举管理。

**三种特殊房间**：

| 房间 | 图标 | 效果 | 复用系统 |
|------|------|------|----------|
| **祭坛** (ALTAR) | `+` 琥珀色地板 | 4 结果池随机（25%）：攻击赐福 / 治愈赐福 / 代价换力量 / 净化负面 | Buff / Heal |
| **宝箱** (TREASURE) | `$` 深蓝色地板 | 品质分层（60% 普通 / 30% 丰厚 2 件 / 10% 祝福 +attack_up） | 物品生成 / Inventory |
| **泉水** (FOUNTAIN) | `~` 深绿色地板 | 回满 HP + 净化 poison/slow | Heal / Buff |

**交互规则**：
- 首次走入房间 → 弹出发现提示（2.5 秒淡出，如"你发现了一座古老祭坛。"）
- 站在房间内按 **E** → 触发奖励（优先于拾取）
- 每个房间每层只能触发一次，触发后地板灰化
- 发现状态（`discovered`）和触发状态（`triggered`）独立存档

**Seed 驱动地图恢复**：
- 每层生成时记录 `dungeon_seed`
- 存档保存 `seed` + `spr`（已触发）+ `spd`（已发现）
- 读档用同 seed 确定性重建地图 → 恢复所有房间状态

**结构**（`src/game/world/special_room.h/.cpp`）：
- `SpecialRoomType` enum + `SpecialRoom` struct（cx/cy/rx/ry/rw/rh/type/discovered/triggered）
- `GameMap::special_rooms` + `get_special_room_at(tx, ty)` 查询
- `DungeonGenerator` 排除玩家/楼梯房间后 Fisher-Yates 分配
- `execute_special_room(type, player)` 统一交互入口（GameScene 只调此函数）

### 5.8 地图生成

**BSP（二分空间划分）** 算法随机生成：

- 从地图全区域开始，交替水平/垂直切分
- 叶子节点放置随机房间 + L 形走廊连接
- 每次生成 ~12 房间，结构完全不同
- 玩家出生在第一个房间，Boss/楼梯在最远房间

---

## 六、项目结构

```
roguelike_cpp/
├── CMakeLists.txt          # CMake 构建配置
├── README.md               # 本文档
├── main.cpp                # 程序入口 + 字体/音频初始化
├── assets/
│   └── jojo_timestop.mp3   # The World 音效
├── vendor/
│   ├── raylib/             # Raylib 5.0 预编译包
│   └── json/               # nlohmann/json (可选)
├── src/
│   ├── core/               # Godot 风格引擎框架
│   │   ├── object.h        #  Object: 信号槽基类
│   │   ├── node.h / .cpp   #  Node: 场景树节点 + 生命周期
│   │   ├── scene_tree.h / .cpp  # SceneTree: 主循环 + 场景切换
│   │   ├── input_map.h     #  InputMap: 抽象动作→按键映射
│   │   ├── logger.h        #  Logger: game.log + crash.log
│   │   ├── win_center.h/.cpp  # 窗口居中 (Win32 API)
│   │   └── seh_handler.cpp #  SEH 崩溃捕获
│   └── game/
│       ├── config.h        #  全局常量
│       ├── entities/       #  实体模块（组合模式）
│       │   ├── entity.h    #  Entity 基类（位置+碰撞框）
│       │   ├── combat_stats.h  # CombatStats 战斗组件
│       │   ├── types.h     #  Vec2/RectF/Color/Effect 基础类型
│       │   ├── player.h / .cpp  # Player: Entity + Stats + Inventory + Skills
│       │   ├── monster.h / .cpp # Monster: Entity + Stats + AI
│       │   ├── ai.h / .cpp      # MonsterAI: IDLE/CHASE/ATTACK 状态机
│       │   ├── boss.h / .cpp    # BossAI: 继承MonsterAI + 3BossSkill + 狂暴
│       │   ├── item.h / .cpp    # Item层次: Equipment/Consumable/Charm + 4Rarity
│       │   ├── skill.h / .cpp   # Skill层次: 4Active + 2Passive + SkillManager + 3级升级
│       │   └── inventory.h/.cpp # Inventory: 背包+装备+使用
│       ├── systems/        #  游戏系统（纯函数）
│       │   ├── combat_system.h /.cpp  # calculate_damage / find_target / cone
│       │   └── vfx_server.h / .cpp    # 攻击特效 (pulse/spark/bolt/flash/slash_arc)
│       ├── world/          #  世界模块
│       │   ├── game_map.h / .cpp            # GameMap: 2D瓦片网格+碰撞
│       │   ├── dungeon_generator.h / .cpp   # BSPNode + BSP生成器 + seed驱动
│       │   ├── special_room.h / .cpp          # 特殊房间: 3类型/交互/发现提示
│       ├── audio/          #  音频模块（程序化合成）
│       │   ├── wave_synth.h / .cpp   # 波形合成 (方波/正弦/噪声/ADSR)
│       │   ├── bgm_engine.h / .cpp   # 4支BGM (和弦+旋律+低音+鼓轨)
│       │   └── audio_server.h / .cpp # AudioServer: SFX+BGM统一管理
│       ├── scenes/         #  场景模块（替代原状态机）
│       │   ├── title_scene.h / .cpp         # 标题画面
│       │   ├── game_scene.h / .cpp          # 核心游戏场景 (~26KB)
│       │   ├── floor_select_scene.h / .cpp  # 选关界面
│       │   ├── tutorial_scene.h / .cpp      # 教程场景
│       │   ├── death_scene.h / .cpp         # 死亡画面
│       │   └── victory_scene.h / .cpp       # 通关画面
│       ├── save/           #  存档模块
│       │   └── save_manager.h / .cpp  # JSON存档/读档 (complete)
│       └── tutorial/       #  教程逻辑
│           └── tutorial_guide.h / .cpp  # 7阶段教程引导
└── resources/              #  JSON 资源配置（外部数据）
```

---

## 七、架构设计

### 7.1 Godot 风格引擎框架

本 C++ 版模仿 Godot 引擎的节点树/信号/场景架构：

| 概念 | 实现 |
|------|------|
| **Object** | 信号槽基类 `Signal<T>` — 类型安全的事件系统 |
| **Node** | 树节点 — 生命周期 (_enter_tree / _ready / _process / _input) |
| **SceneTree** | 根节点 + 主循环 — 60FPS Ticker + 场景切换 |
| **InputMap** | 动作→按键映射 — 游戏逻辑不关心具体按键 |
| **Scene** | 独立场景类 — 替代 if-elif 状态分发 |

### 7.2 场景树（替代 Python 版状态机）

```
SceneTree (root)
├── TitleScene          — N/C/F/T/Esc 菜单
├── GameScene           — 核心游戏（地图/实体/VFX/HUD/背包）
├── FloorSelectScene    — 15关选关面板
├── TutorialScene       — 7阶段教程
├── BossIntro (内嵌)    — Boss背景介绍面板
├── DeathScene          — 死亡画面
└── VictoryScene        — 通关画面
```

### 7.3 类层次

```
Object (信号槽)
  └── Node (场景树节点)
        ├── TitleScene
        ├── GameScene
        ├── FloorSelectScene
        ├── TutorialScene
        ├── DeathScene
        └── VictoryScene

Entity (位置+碰撞框)          CombatStats (HP/ATK/DEF/modifiers)
  └── Player                    ├── Monster (AI组件)
   (组合) ├── Inventory         │     └── MonsterAI (IDLE→CHASE→ATTACK)
           ├── SkillManager         └── BossAI (继承MonsterAI + BossSkill×3)
           └── CombatStats

Item (稀有度+属性)
  ├── EquipmentItem (武器/护甲)       Skill (冷却+等级+护符)
  ├── CharmItem (护符)                 ├── ActiveSkill
  └── ConsumableItem (药水)            │     ├── SlashSkill
                                         │     ├── FireballSkill
                                         │     ├── SelfHealSkill
                                         │     └── TheWorldSkill
                                         └── PassiveSkill
                                               ├── IronSkinSkill
                                               └── BerserkSkill
```

### 7.4 信号系统（解耦机制）

```cpp
// 定义信号
Object::Signal<int, const char*> on_player_leveled;
Object::Signal<Monster*> on_monster_killed;

// 连接
on_monster_killed.connect([](Monster* m) {
    drop_system->spawn_loot(m);
    xp_system->award_xp(m);
    vfx_server->death_particles(m);
});

// 发射
on_monster_killed.emit(target);
```

### 7.5 设计模式

| 模式 | 应用 | 说明 |
|------|------|------|
| **组件模式** | Entity + CombatStats + MonsterAI | 组合优于继承 |
| **场景树** | SceneTree + 7 Scene | 替代 Python 版状态机 dispatch table |
| **信号槽** | Object::Signal<T> | 解耦：战斗→掉落→XP→VFX→存档，各不依赖 |
| **Factory** | spawn_monster / spawn_boss / random_skill | 统一创建，隔离构造逻辑 |
| **Strategy** | MonsterAI → BossAI (继承替换) | Boss 挂载派生 AI 获得特殊行为 |
| **Singleton** | AudioServer / SaveManager | 全局音频/存档服务 |

### 7.6 伤害公式

```
最终伤害 = max(1, (攻方ATK − 守方DEF × 0.5) × (±20%浮动))

DEF 按攻击类型选择:
  AttackType::PHYSICAL → physical_defense   + pdef_flat (装备)
  AttackType::MAGICAL  → magical_defense    + mdef_flat (装备)
  AttackType::TRUE     → 0 (无视防御)

modifiers 叠加:
  "atk_flat"  ← 武器装备 / 被动技能
  "def_flat"  ← 铁壁被动 / 护甲装备
  "pdef_flat" ← 物理护甲
  "mdef_flat" ← 魔法护甲
```

### 7.7 技能升级表

| 技能 | Lv1 | Lv2 | Lv3 |
|------|-----|-----|-----|
| 斩击 | CD 2s, 1格, ATK×1.5 | CD 1.6s, 2格, ×1.4 | CD 1.2s, 2格, 二连击 ×2.0 |
| 神罚 | CD 5s, 1目标, ATK×2.5 | CD 4s, 2目标, ×1.4 | CD 3s, 3目标, ×2.0 |
| 自愈 | CD 8s, 瞬回20% | CD 6.5s, 瞬回35% | CD 5s, 瞬回35% + 4秒再生 |
| The World | CD 20s, 停5s | CD 16s, 停7s | CD 12s, 停10s |
| 铁壁 | DEF+3 | DEF+7 | DEF+12 |
| 狂暴 | ATK+3 | ATK+7 | ATK+12 |

### 7.8 音频系统

**SFX**（8 种，纯程序化方波/正弦/噪声合成）：
`melee` `hit` `slash` `bolt` `heal` `pickup` `levelup` `victory` + `timestop` (外部 MP3)

**BGM**（4 支，和弦+旋律+低音+鼓轨，3遍循环 ~25s）：
`title` (C大调/110BPM/管弦) `select` (Am/85BPM/竖琴) `dungeon` (75BPM/暗流) `boss` (150BPM/重金属)

### 7.9 日志系统

- **game.log**：60+ 条覆盖点 — 启动/音频/字体/场景/楼层/Boss/战斗/升级/拾取/存档/死亡/通关/退出
- **crash.log**：SEH 异常 + C++ terminate — 独立编译单元，崩溃时直接写文件不依赖 Logger

### 7.10 难度曲线

```
层 1-4:  HP×1.0→1.5   ATK×1.0→1.4   (逐渐升温)
层 5:    Boss 暗影骑士 (HP:250 ATK:15)
层 6-9:  HP×1.6→2.2   ATK×1.5→2.0   (地狱前庭)
层 10:   Boss 地狱火魔 (HP:500 ATK:24)
层 11-14: HP×2.4→3.3   ATK×2.2→2.9   (深渊深处)
层 15:   Boss 深渊之主·终焉 (HP:900 ATK:35) → 通关！
```

---

## 八、与 Python 版的对比

| 方面 | Python (pygame) | C++ (raylib) |
|------|-----------------|---------------|
| 状态管理 | 单一 GameEngine + dispatch table | SceneTree + 7 Scene 类 |
| 模块通信 | 直接 import + 方法调用 | Signal 信号槽 |
| 内存管理 | GC | shared_ptr / unique_ptr (RAII) |
| 渲染 | pygame.draw.rect/circle | raylib DrawRectangle/DrawCircle |
| 字体 | TTF 自动（pygame 内置） | LoadFontEx + 逐字码位加载（816字） |
| 音频 | pygame.mixer.Sound | Wave 合成 → LoadSoundFromWave |
| 输入 | pygame.key.get_pressed() | InputMap 动作→按键抽象 |
| 打包 | PyInstaller (37MB) | 原生 EXE + raylib.dll (2MB) |
| 架构风格 | — | Godot Node/SceneTree 启发的游戏框架 |

---

## 九、存档格式

采用简洁的 `key:value` 文本格式（`saves/save.json`）：

```
v:1
floor:11
maxf:11
lv:5
xp:50
xpt:720
mhp:140
chp:140
atk:14
pd:4
md:2
act:Slash,2;Fireball,1;SelfHeal,1;TheWorld,1;
pas:IronSkin,2;Berserk,1;
inv:长剑,RARE,weapon,8,1,0;生命药水,COMMON,heal,20;
eqw:魔渊之刃,LEGENDARY,weapon,18,3,0
eqa:铁铠,RARE,armor,0,5,1
buf:poison,2,3.50,0.20;attack_up,1,5.80,0.00;
seed:2873910246
spr:1,0,1
spd:1,1,0
```

保存内容：楼层/等级/属性/主动技能(名+等级)/被动技能/背包物品/装备/Buff/地牢种子/特殊房间触发+发现状态。

---

## 十、开发进度

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
