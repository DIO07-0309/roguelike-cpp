# 仿以撒圣遗物系统设计文档

> **目标**：将遗物从"散装数值被动"升级为"改造核心机制的构建模块"，使每局游戏产生独特的 build 身份感。
>
> **参考**：The Binding of Isaac: Repentance 遗物/套装/协同系统
>
> **前置条件**：当前 60 个遗物 + BuffTrigger 事件系统 + EventBus 45 事件

---

## 一、现状分析

### 1.1 当前遗物架构

```
RelicInstance { id, from_boss }    ← 只有 ID，无运行时状态
        ↓
player_has_relic(player, "xxx")    ← 硬编码散落在 10+ 个 .cpp 文件
        ↓
if (venom_fang)  poison_dmg += 1   ← 每个遗物一个 if 分支
if (war_drum)    atk += 0.15
if (hunter_gloves) atk += 0.10
...
```

**问题**：
- 遗物效果与 C++ 代码强耦合 → 新增遗物必须改代码
- 遗物之间无交互 → 60 个遗物各自独立
- 不改变核心手感 → 只加数值（+ATK/+DEF/+HP），不改"怎么打"
- 无套装/变形系统 → 缺少长期构建目标

### 1.2 以撒遗物系统对比

| 维度 | 以撒 | 你的项目 |
|------|------|---------|
| 效果类型 | 改变攻击模式/弹道/移速/机制 | 只加数值 |
| 协同 | 道具之间有组合效果 | 无 |
| 套装 | 集齐 N 个同系列 → 变形 | 无 |
| 身份感 | "这局是激光流" | "这局有5个散装被动" |
| 数据化 | 全部 JSON 驱动 | 效果硬编码在 C++ |

---

## 二、系统设计

### 2.1 架构总览

```
┌─────────────────────────────────────────────────┐
│                 RelicEffectSystem                │
│  ┌───────────┐  ┌───────────┐  ┌──────────────┐│
│  │ 触发器引擎 │  │ 套装检测器 │  │ 协同网络查询 ││
│  └─────┬─────┘  └─────┬─────┘  └──────┬───────┘│
│        │              │               │         │
│   ┌────▼──────────────▼───────────────▼────┐    │
│   │         EventBus (45 事件)              │    │
│   └────────────────────────────────────────┘    │
└─────────────────────────────────────────────────┘
         ↕                          ↕
┌────────────────┐        ┌────────────────┐
│  RelicRegistry │        │ Player/Combat  │
│  (JSON → Def)  │        │ (运行时状态)    │
└────────────────┘        └────────────────┘
```

### 2.2 遗物效果数据结构

在 `resources/relics.json` 中为每个遗物增加效果描述字段：

```json
{
  "id": "venom_fang",
  "name": "毒牙",
  "rarity": "common",
  "param": 1,
  "set": "poison",
  "effects": [
    {
      "trigger": "on_hit",
      "type": "add_damage",
      "element": "poison",
      "value": 1,
      "desc": "毒伤每层 +1"
    },
    {
      "trigger": "passive",
      "type": "modify_stat",
      "stat": "poison_power",
      "value": 0.2,
      "desc": "毒伤害 +20%"
    }
  ],
  "synergy": [
    {
      "requires": ["frozen_crystal"],
      "effect": {
        "trigger": "on_hit",
        "type": "element_combo",
        "combo": "poison+ice",
        "desc": "冰冻目标被毒伤时立即结算剩余毒伤"
      }
    }
  ]
}
```

### 2.3 触发器类型（复用现有 BuffTrigger 机制）

| 触发器 | 触发时机 | 对应 EventBus 事件 |
|--------|---------|-------------------|
| `on_hit` | 攻击命中敌人时 | PLAYER_ATTACK |
| `on_kill` | 击杀敌人时 | MONSTER_DIED |
| `on_hurt` | 受到伤害时 | PLAYER_DAMAGED |
| `on_floor` | 进入新楼层时 | FLOOR_ENTER |
| `on_skill` | 使用技能时 | （新增 SKILL_USED 事件） |
| `on_combo` | 连击达到特定段时 | （新增 COMBO_MILESTONE 事件） |
| `passive` | 常驻效果 | 无（每帧 tick） |
| `on_dead` | 死亡时 | PLAYER_DEAD |

### 2.4 效果类型

| 类型 | 说明 | 示例 |
|------|------|------|
| `add_damage` | 增加固定伤害 | 毒伤 +1 |
| `multiply_damage` | 伤害乘算 | ATK ×1.15 |
| `modify_stat` | 修改属性值 | 攻速 +20% |
| `modify_mechanic` | **修改核心机制**（重点） | 攻击穿透 / 弹道分裂 / 连击段数 +1 |
| `grant_buff` | 获得 Buff | 击杀获得攻速 Buff |
| `spawn_projectile` | 生成额外弹体 | 攻击附带追踪弹 |
| `heal` | 回复 HP | 击杀回复 5 HP |
| `teleport` | 位移效果 | 受击时后退 2 格 |
| `element_combo` | 元素联动 | 冰+毒=立即结算 |

### 2.5 核心机制改造（最重要）

以下效果可以直接改造玩家的战斗手感：

| 效果 | 实现方式 | 影响的代码 |
|------|---------|-----------|
| **攻速修改** | `attack_cooldown *= modifier` | player.h ATTACK_COOLDOWN 改为成员变量 |
| **连击段数 +1** | `max_combo += 1` | ComboState 结构体 |
| **攻击穿透** | HitResult 增加 `pierce_count` | hit_detection.cpp |
| **弹道分裂** | 命中后 spawn 追踪弹 | projectile_factory.cpp |
| **攻击范围扩大** | 命中形状尺寸 × modifier | hit_detection 各形状参数 |
| **移动攻击** | 攻击时可移动 | player_controller.cpp 攻击状态锁 |
| **自动闪避** | 受击时概率触发无敌帧 | combat_system.cpp 伤害计算 |
| **击杀刷新** | 击杀重置技能冷却 | skill.cpp cooldown 系统 |

---

## 三、套装/变形系统

### 3.1 套装定义

在 `resources/relics.json` 中用 `set` 字段标记套装成员：

```json
[
  {"id": "guppy_eye", "set": "guppy", ...},
  {"id": "guppy_tail", "set": "guppy", ...},
  {"id": "guppy_heart", "set": "guppy", ...}
]
```

### 3.2 套装效果表

| 套装 | 成员数 | 变形效果 |
|------|--------|---------|
| **亡灵** (necro) | 3 | 召唤骷髅随从（每层 1 个，继承玩家 ATK×0.5） |
| **凤凰** (phoenix) | 3 | 死亡时复活（1 HP + 3 秒无敌），每层 1 次 |
| **冰霜** (frost) | 3 | 攻击冻结概率 +50%，冻结持续时间 ×2 |
| **雷霆** (thunder) | 3 | 攻击连锁闪电（弹射 3 次，每次衰减 30%） |
| **暗影** (shadow) | 3 | 击杀后隐身 2 秒，下一次攻击暴击 |
| **狂战** (berserker) | 3 | HP 越低攻速越快（线性，最低 0.2s 间隔） |
| **时间** (chrono) | 3 | The World 时停冷却 -50%，时停结束时敌人减速 2 秒 |
| **毒师** (plague) | 3 | 毒伤无视防御，毒层数上限 +5 |

### 3.3 检测逻辑

```cpp
// RelicEffectSystem::check_sets()
void RelicEffectSystem::check_sets(Player* player) {
    std::unordered_map<std::string, int> set_counts;
    for (auto& r : player->relics)
        if (!r.set.empty()) set_counts[r.set]++;

    for (auto& [set_id, count] : set_counts) {
        if (count >= 3 && !_active_sets.count(set_id)) {
            _activate_set(set_id, player);  // 首次凑齐
            _active_sets.insert(set_id);
        } else if (count < 3 && _active_sets.count(set_id)) {
            _deactivate_set(set_id, player); // 丢失成员
            _active_sets.erase(set_id);
        }
    }
}
```

---

## 四、协同网络

### 4.1 协同规则

两个遗物 A 和 B 之间存在协同时，需要**同时持有**才激活：

```json
{
  "id": "frozen_crystal",
  "synergy": [
    {
      "requires": ["venom_fang"],
      "effect": {
        "trigger": "on_hit",
        "type": "element_combo",
        "combo": "poison+ice",
        "desc": "冰冻目标被毒伤时立即结算剩余毒伤"
      }
    }
  ]
}
```

### 4.2 协同网络图（20 组高感知度协同）

```
冰霜系列                 雷霆系列                 血系列
├─ frozen_crystal ──── thunder_core              ├─ blood_chalice
│   × venom_fang = 毒冰联动                      │   × leech_blade = 吸血翻倍
│   × ice_nova skill = 冰爆范围+50%              │   × blood_frenzy = 血怒无消耗
│                                                │
├─ absolute_zero ──── storm_caller               ├─ phoenix_feather
│   × any ice = 冻结后AOE                       │   × blood_chalice = 浴火回血
│                                                │   × soul_lantern = 复活时满层buff

时间系列                   暗影系列                 元素混合
├─ time_fragment ──── shadow_cloak               ├─ venom_fang
│   × the_world skill = 时停中可移动             │   × frozen_crystal = 毒冰联动
│   × chrono set = 时停CD-50%                    │   × thunder_core = 毒雷AOE
│                                                │
├─ paradox_orb ───── assassin_mark               ├─ phoenix_feather
│   × time_fragment = 时停结束全屏伤害           │   × frozen_crystal = 冰火对冲(范围爆炸)
│   × chrono set = 双时停                        │   × thunder_core = 雷火=闪电风暴
```

---

## 五、代码改造路径

### 5.1 第一阶段：遗物效果数据化（难度低）

**目标**：消除 `player_has_relic()` 硬编码，改用统一触发器引擎。

| 步骤 | 改动 | 文件 |
|------|------|------|
| 1 | RelicInstance 增加 `effects` 向量 | combat_stats.h |
| 2 | 新增 RelicEffectProcessor：注册 EventBus 监听 → 遍历遗物 effects → 执行 | 新文件 relic_effect_processor.h/cpp |
| 3 | JSON 加载时解析 effects 字段 | relic_defs.h（新增加载器） |
| 4 | 逐个替换 player_has_relic 硬编码 → 改为 EventBus 触发 | combat_system.cpp 等 |
| 5 | 删除所有 player_has_relic 调用 | 全局搜索替换 |

**影响范围**：
- `src/game/entities/combat_stats.h` — RelicInstance 结构体
- `src/game/systems/combat_system.cpp` — 10 处 player_has_relic
- `src/game/scene/game_scene_combat.cpp` — 8 处 player_has_relic
- `src/game/systems/combat_coordinator.cpp` — 2 处 player_has_relic
- 新增 `src/game/systems/relic_effect_processor.h/cpp`

### 5.2 第二阶段：核心机制可修改（难度中）

**目标**：让遗物能改变攻击间隔、连击段数、弹道形态等核心参数。

| 步骤 | 改动 | 文件 |
|------|------|------|
| 1 | ATTACK_COOLDOWN 从 constexpr 改为成员变量 | player.h |
| 2 | ComboState 增加 max_combo 成员 | player.h |
| 3 | HitResult 增加 pierce_count | hit_detection.h |
| 4 | WeaponExecutor 支持"基础模式 + 遗物修饰符" | weapon_executor.cpp |
| 5 | 新增 ModifyMechanicEffect 类型 | relic_effect_processor.h |

### 5.3 第三阶段：套装 + 协同（难度高）

**目标**：实现套装变形和遗物协同网络。

| 步骤 | 改动 | 文件 |
|------|------|------|
| 1 | RelicInstance 增加 set 字段 | combat_stats.h |
| 2 | RelicEffectSystem 增加套装检测器 | relic_effect_processor.h |
| 3 | RelicInstance 增加 synergy 向量 | combat_stats.h |
| 4 | 协同网络查询逻辑 | relic_effect_processor.cpp |
| 5 | 变形 UI 提示（类似以撒的变形动画） | game_renderer.cpp |

---

## 六、JSON 配置规范

### 6.1 遗物完整字段

```json
{
  "id": "venom_fang",
  "name": "毒牙",
  "short_name": "毒",
  "desc": "毒伤每层 +1",
  "rarity": "common",
  "param": 1,
  "set": "poison",
  "hud_color": [100, 220, 80],
  "effects": [
    {
      "trigger": "on_hit",
      "type": "add_damage",
      "element": "poison",
      "value": 1,
      "chance": 1.0,
      "desc": "毒伤每层 +1"
    }
  ],
  "synergy": [
    {
      "requires": ["frozen_crystal"],
      "desc": "冰冻目标被毒伤时立即结算",
      "effect": {
        "trigger": "on_hit",
        "type": "element_combo",
        "combo": "poison+ice"
      }
    }
  ]
}
```

### 6.2 套装配置

```json
{
  "sets": {
    "necro": {
      "name": "亡灵召唤",
      "members_required": 3,
      "effect": {
        "type": "summon",
        "summon_type": "skeleton",
        "count": 1,
        "inherit_atk_pct": 0.5,
        "desc": "召唤骷髅随从"
      }
    },
    "phoenix": {
      "name": "凤凰涅槃",
      "members_required": 3,
      "effect": {
        "type": "revive",
        "revive_hp": 1,
        "invincible_duration": 3.0,
        "uses_per_floor": 1,
        "desc": "死亡时复活"
      }
    }
  }
}
```

---

## 七、验证标准

| 指标 | 目标 |
|------|------|
| 新增遗物无需改 C++ | 纯 JSON 配置即可添加新遗物 |
| 300 局模拟胜率 | 仍保持 6-10% 区间 |
| 构建多样性 | 每局至少出现 2 种不同 build 身份 |
| 套装触发率 | 平均每 3 局触发 1 次套装变形 |
| 协同触发率 | 平均每局触发 1-2 次协同效果 |

---

## 八、里程碑规划

| 阶段 | 内容 | 工作量 |
|------|------|--------|
| **M1** | RelicEffectProcessor + JSON effects 解析 + 替换硬编码 | 2-3 天 |
| **M2** | 核心机制改造（攻速/连击/穿透） | 2 天 |
| **M3** | 套装系统（8 套装 × 3 成员） | 2 天 |
| **M4** | 协同网络（20 组协同） | 2 天 |
| **M5** | 数值平衡 + 300 局回归 | 1 天 |
| **合计** | | **~10 天** |

---

> **设计哲学**：以撒的圣遗物不只是"加 10% 攻击力"，而是"这局你的攻击变成了激光"。
> 我们的目标是让玩家在拾取遗物时能感受到"我的玩法变了"，而不只是"我的数字大了"。
