# M1A.1 RelicEffect Foundation — Implementation Plan (v2)

> 前置：P0 Bug 修复 + P0.5 Balance Audit 已通过
> 目标：建立 RelicEffect 运行时基础设施，5 个 Ghost Relics 完整闭环
> 原则：新旧系统双轨并行，Ghost Relics 只走新路径，JSON 为 Single Source of Truth

---

## A. 架构总览

### A.1 数据流

```
resources/relics.json (effects[] array, optional)
        ↓  load_relic_defs() 扩展解析
        ↓
g_relic_defs[id].effects  →  RelicEffectDef[]  (immutable)
        ↓
RelicEffectProcessor (显式入口调用，不订阅 EventBus)
        ↓  on_kill() / on_hit() / on_pre_damage() / on_floor_enter() / tick()
        ↓  遍历 player->relics → 匹配 trigger → 查 effects[] → Handler
        ↓
效果应用 (stat / buff / heal / damage / ...)
```

### A.2 PRE_DAMAGE 流程

```
伤害发生
  → DamageContext { source, target, raw_damage, final_damage, damage_type, cancelled }
  → RelicEffectProcessor::on_pre_damage(DamageContext&)
  → Handler 修改 final_damage (如 tiny_shield 减伤)
  → take_damage(final_damage)
  → RelicEffectProcessor::on_hurt(player, damage)
```

### A.3 新旧双轨

```
旧路径 (保留，逐步废弃)              新路径 (新增)
player_has_relic() → if-chain       RelicEffectProcessor → Handler
23 个已实现遗物                     Ghost Relics (effects[] 非空)
RelicDef.param/param2               RelicDef.effects[] 数组
```

**共存规则**：
1. Ghost Relics 无硬编码 → effects[] 非空 → 新路径
2. 现有 23 个遗物有硬编码 → effects[] 为空 → 旧路径
3. 同一 relic 不走两条路径

---

## B. 数据结构设计

### B.1 DamageContext（新增）

新增文件：`src/game/systems/damage_context.h`

```cpp
#pragma once
#include <string>

enum class DamageType {
    PHYSICAL,
    MAGICAL,
    POISON,
    FIRE,
    ICE,
    LIGHTNING,
    TRUE,           // 无视防御
};

struct DamageContext {
    void* source = nullptr;        // 攻击来源 (Player*/Monster*/nullptr for hazard)
    void* target = nullptr;        // 受击目标 (Player*/Monster*)
    int raw_damage = 0;            // 原始伤害
    int final_damage = 0;          // 最终伤害 (可被 PRE_DAMAGE handler 修改)
    DamageType damage_type = DamageType::PHYSICAL;
    bool cancelled = false;        // 设为 true 则取消伤害
    bool is_lethal = false;        // 是否致死
};
```

### B.2 RelicEffectDef（静态定义）

新增文件：`src/game/systems/relic_effect.h`

```cpp
#pragma once
#include <string>

enum class RelicTrigger {
    PASSIVE,          // 常驻效果（每帧 tick）
    ON_HIT,           // 攻击命中敌人时
    ON_KILL,          // 击杀敌人时
    PRE_DAMAGE,       // 受到伤害前（可修改 DamageContext）
    ON_HURT,          // 受到伤害后（不可修改）
    ON_FLOOR_ENTER,   // 进入新楼层时
    ON_SKILL_CAST,    // 使用技能时
    ON_DEATH,         // 死亡时
};

enum class RelicEffectType {
    MODIFY_STAT,      // 修改属性值
    ADD_BUFF,         // 获得 Buff
    HEAL,             // 回复 HP
    DEAL_AOE_DAMAGE,  // 造成范围伤害
    DAMAGE_REDUCTION, // 减伤百分比 (PRE_DAMAGE only)
    RESET_COOLDOWNS,  // 刷新所有技能冷却
    SPAWN_PROJECTILE, // 生成额外弹体
};

enum class RelicTarget {
    SELF,
    NEAREST_ENEMY,
    ALL_ENEMIES,
};

struct RelicEffectDef {
    RelicTrigger trigger = RelicTrigger::PASSIVE;
    RelicEffectType type = RelicEffectType::MODIFY_STAT;
    RelicTarget target = RelicTarget::SELF;

    std::string stat;      // MODIFY_STAT: "attack" / "speed" / "max_hp" / "physical_defense" / ...
    float value = 0.0f;    // 通用数值参数
    int   value2 = 0;      // 整数参数
    std::string buff_id;   // ADD_BUFF: buff identifier
    float chance = 1.0f;   // 触发概率 [0,1]
};
```

### B.3 RelicDef 扩展

在现有 `RelicDef`（combat_system.h:93-106）中增加 effects 字段：

```cpp
struct RelicDef {
    // ... 现有字段不变 ...
    // M1A.1 新增
    std::vector<RelicEffectDef> effects;  // 数据驱动效果列表
};
```

### B.4 RelicEffectRuntime（运行时状态，严格分离）

新增文件：`src/game/systems/relic_effect_runtime.h`

```cpp
#pragma once
#include <string>
#include <unordered_map>

// 每个遗物的运行时计数器/冷却 (mutable, per-run)
struct RelicEffectState {
    float timer = 0.0f;       // PASSIVE_TIMER 类冷却计时
    int   charges = 0;        // 充能次数
    bool  activated = false;  // 一次性效果是否已触发
};

// 玩家所有遗物的运行时状态
struct RelicEffectRuntime {
    std::unordered_map<std::string, RelicEffectState> states;

    RelicEffectState& get(const std::string& relic_id) {
        return states[relic_id];
    }

    void reset() { states.clear(); }
};
```

**关键约束**：RelicDef / RelicEffectDef = immutable definition；RelicEffectRuntime = per-run mutable state。不在 RelicDef 中存储 cooldown/charges。

---

## C. RelicEffectProcessor（核心调度器）

新增文件：`src/game/systems/relic_effect_processor.h` + `.cpp`

### C.1 Header

```cpp
#pragma once
#include "relic_effect.h"
#include "relic_effect_runtime.h"
#include "damage_context.h"
#include <vector>

class Player;
class Monster;

class RelicEffectProcessor {
public:
    void set_enabled(bool e) { _enabled = e; }
    bool is_enabled() const { return _enabled; }
    void reset_runtime() { _runtime.reset(); }

    // 显式入口 — 不订阅 EventBus，由调用方显式调用
    void on_kill(Player* player, Monster* monster, std::vector<Monster*>& all_monsters);
    void on_hit(Player* player, Monster* target);
    void on_pre_damage(DamageContext& ctx, Player* player);
    void on_hurt(Player* player, int final_damage);
    void on_floor_enter(Player* player);
    void tick(Player* player, float dt);

private:
    bool _enabled = true;
    RelicEffectRuntime _runtime;

    void _apply_passive_stat(Player* player, const RelicEffectDef& eff, const std::string& relic_id);
    void _apply_on_kill(Player* player, Monster* monster, const RelicEffectDef& eff);
    void _apply_on_hit(Player* player, Monster* target, const RelicEffectDef& eff);
    void _apply_pre_damage(DamageContext& ctx, const RelicEffectDef& eff);
    void _apply_on_floor_enter(Player* player, const RelicEffectDef& eff);
};
```

### C.2 调用方式（显式，不订阅 EventBus）

```cpp
// 在 GameSceneCombat 或 game_scene.cpp 中显式调用：
_relic_fx.on_kill(player.get(), monster.get(), monsters);
_relic_fx.on_hit(player.get(), target);
_relic_fx.on_pre_damage(ctx, player.get());
_relic_fx.on_hurt(player.get(), final_damage);
_relic_fx.on_floor_enter(player.get());
_relic_fx.tick(player.get(), dt);
```

---

## D. JSON 解析扩展

### D.1 effects 数组 Schema

```json
{
  "id": "iron_ring",
  "effects": [
    {
      "trigger": "passive",
      "type": "modify_stat",
      "stat": "physical_defense",
      "value": 5,
      "desc": "物理防御+5"
    }
  ]
}
```

### D.2 解析代码位置

在现有 `combat_system.cpp` 的 `_parse_relic_obj()` 函数中扩展。

### D.3 字段映射

| JSON 字段 | C++ 字段 | 类型 | 必填 |
|-----------|---------|------|------|
| `trigger` | `RelicEffectDef::trigger` | string→enum | 是 |
| `type` | `RelicEffectDef::type` | string→enum | 是 |
| `target` | `RelicEffectDef::target` | string→enum | 否 (SELF) |
| `stat` | `RelicEffectDef::stat` | string | MODIFY_STAT 时是 |
| `value` | `RelicEffectDef::value` | float | 否 (0) |
| `value2` | `RelicEffectDef::value2` | int | 否 (0) |
| `buff_id` | `RelicEffectDef::buff_id` | string | ADD_BUFF 时是 |
| `chance` | `RelicEffectDef::chance` | float | 否 (1.0) |

---

## E. 5 个 Handler（M1A.1 范围）

### E.1 PASSIVE + MODIFY_STAT

**覆盖**：iron_ring (PDEF+5)

```cpp
void RelicEffectProcessor::_apply_passive_stat(
    Player* player, const RelicEffectDef& eff, const std::string& relic_id)
{
    if (eff.stat == "physical_defense") {
        player->combat.physical_defense += static_cast<int>(eff.value);
    } else if (eff.stat == "attack") {
        // 被动 ATK 修改 — M1A.1 暂不处理（避免与旧系统重复）
    }
    // ... 其他 stat 类型
}
```

### E.2 ON_KILL

**覆盖**：thunder_orb (击杀 30% 连锁闪电 AOE)

```cpp
void RelicEffectProcessor::_apply_on_kill(
    Player* player, Monster* monster, const RelicEffectDef& eff)
{
    if (rng_float() > eff.chance) return;

    if (eff.type == RelicEffectType::DEAL_AOE_DAMAGE) {
        int dmg = static_cast<int>(player->combat.get_effective_attack() * eff.value);
        for (auto& m : all_monsters) {
            if (m.get() != monster && m->combat.is_alive) {
                m->combat.take_damage(dmg);
            }
        }
    }
}
```

### E.3 ON_HIT

**覆盖**：frozen_heart (攻击 25% 附带冻伤 buff)

```cpp
void RelicEffectProcessor::_apply_on_hit(
    Player* player, Monster* target, const RelicEffectDef& eff)
{
    if (rng_float() > eff.chance) return;

    if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(target, eff.buff_id, eff.value2);
    }
}
```

### E.4 PRE_DAMAGE

**覆盖**：tiny_shield (10% 概率减伤 50%)

```cpp
void RelicEffectProcessor::_apply_pre_damage(
    DamageContext& ctx, const RelicEffectDef& eff)
{
    if (eff.type == RelicEffectType::DAMAGE_REDUCTION) {
        if (rng_float() < eff.chance) {
            ctx.final_damage = static_cast<int>(ctx.final_damage * (1.0f - eff.value));
        }
    }
}
```

### E.5 ON_FLOOR_ENTER

**覆盖**：emerald_heart (进入新楼层时获得 regen buff)

```cpp
void RelicEffectProcessor::_apply_on_floor_enter(
    Player* player, const RelicEffectDef& eff)
{
    if (eff.type == RelicEffectType::ADD_BUFF) {
        apply_buff(player, eff.buff_id, eff.value2);
    }
}
```

---

## F. 5 个试点 Ghost Relics

| 遗物 | Trigger | Effect Type | JSON data | Gameplay |
|------|---------|-------------|-----------|----------|
| `iron_ring` | PASSIVE | MODIFY_STAT physical_defense +5 | `"stat":"physical_defense","value":5` | 玩家 DEF 实时 +5 |
| `thunder_orb` | ON_KILL | DEAL_AOE_DAMAGE 30% | `"chance":0.3,"value":1.0,"type":"deal_aoe_damage"` | 击杀后 30% 连锁闪电 |
| `frozen_heart` | ON_HIT | ADD_BUFF slow | `"chance":0.25,"buff_id":"slow","value2":1` | 攻击 25% 附带减速 |
| `tiny_shield` | PRE_DAMAGE | DAMAGE_REDUCTION 10%→50% | `"chance":0.10,"value":0.50,"type":"damage_reduction"` | 10% 概率减伤 50% |
| `emerald_heart` | ON_FLOOR_ENTER | ADD_BUFF regen | `"buff_id":"regen","value2":3` | 进入新楼层获得 3 层回血 |

---

## G. 集成点

### G.1 GameSceneCombat 持有实例

```cpp
// game_scene_combat.h
RelicEffectProcessor _relic_fx;
```

### G.2 显式调用点

| 入口 | 调用位置 | 文件 |
|------|---------|------|
| `on_kill()` | `on_monster_killed()` 内 | game_scene_combat.cpp |
| `on_hit()` | `_resolve_one()` 命中后 | weapon_executor.cpp |
| `on_pre_damage()` | `take_damage()` 前 | combat_system.cpp |
| `on_hurt()` | `take_damage()` 后 | combat_system.cpp |
| `on_floor_enter()` | `enter_floor()` 内 | game_scene.cpp |
| `tick()` | buff tick 附近 | game_scene.cpp |

### G.3 on_pre_damage 集成

在 `CombatStats::take_damage()` 中增加 PRE_DAMAGE hook：

```cpp
// combat_stats.h 或 combat_system.cpp
void CombatStats::take_damage_with_relics(
    int raw_damage, DamageType type, void* source,
    RelicEffectProcessor* relic_fx, Player* owner)
{
    DamageContext ctx;
    ctx.source = source;
    ctx.target = this;  // this = Player* 或 Monster* 用 void* 通用处理
    ctx.raw_damage = raw_damage;
    ctx.final_damage = raw_damage;
    ctx.damage_type = type;

    if (relic_fx && owner) {
        relic_fx->on_pre_damage(ctx, owner);  // 修改 final_damage
    }

    if (!ctx.cancelled) {
        take_damage(ctx.final_damage);  // 原有逻辑
    }
}
```

### G.4 去重保证

**证据**：同一 relic 不走两条路径
- iron_ring: JSON effects[] 非空，无 C++ 硬编码 → 仅新路径
- thunder_orb: JSON effects[] 非空，无 C++ 硬编码 → 仅新路径
- frozen_heart: JSON effects[] 非空，无 C++ 硬编码 → 仅新路径
- tiny_shield: JSON effects[] 非空，无 C++ 硬编码 → 仅新路径
- emerald_heart: JSON effects[] 非空，无 C++ 硬编码 → 仅新路径
- 现有 23 个已实现遗物: effects[] 为空 → 仅旧路径

---

## H. 文件变更清单

| 文件 | 操作 | 行数估算 |
|------|------|---------|
| `src/game/systems/damage_context.h` | **新增** | ~30 行 |
| `src/game/systems/relic_effect.h` | **新增** | ~55 行 |
| `src/game/systems/relic_effect_runtime.h` | **新增** | ~25 行 |
| `src/game/systems/relic_effect_processor.h` | **新增** | ~35 行 |
| `src/game/systems/relic_effect_processor.cpp` | **新增** | ~180 行 |
| `src/game/systems/combat_system.h` | 修改 RelicDef | +2 行 |
| `src/game/systems/combat_system.cpp` | 扩展 _parse_relic_obj + take_damage hook | +50 行 |
| `src/game/entities/combat_stats.h` | 增加 take_damage_with_relics | +15 行 |
| `src/game/scene/game_scene_combat.h` | 增加 _relic_fx 成员 | +3 行 |
| `src/game/scene/game_scene_combat.cpp` | on_kill/on_hit 调用 | +10 行 |
| `src/game/scenes/game_scene.cpp` | tick + on_floor_enter 调用 | +5 行 |
| `resources/relics.json` | 5 个 Ghost Relics 增加 effects[] | +25 行 |

**总计**：~435 行（新增 ~360 行，修改 ~75 行）

---

## I. 测试计划

### I.1 单元测试

| 测试 | 内容 |
|------|------|
| damage_context_test | DamageContext 初始化、final_damage 修改 |
| relic_effect_def_test | RelicEffectDef 枚举解析 |
| relic_effect_runtime_test | state get/reset |
| passive_stat_test | iron_ring PDEF +5 真实生效 |
| on_kill_handler_test | thunder_orb 概率 AOE 伤害 |
| on_hit_handler_test | frozen_heart buff 应用 |
| pre_damage_handler_test | tiny_shield 减伤计算 |
| on_floor_enter_handler_test | emerald_heart buff 应用 |
| dual_path_exclusion_test | 证明 effects[] 非空的 relic 无硬编码 |

### I.2 集成验证

- 34 个现有测试全部通过
- `python tools/world_validator.py` 通过
- `--sim 300` 胜率在 baseline 范围
- 5 个 Ghost Relics 可在游戏中正确触发

---

## J. 限制

- 不修改 P0 已确认的伤害/死亡语义
- 不重新调整 weapons.json
- 不订阅 EventBus（M1A.1 使用显式入口）
- 不迁移现有 23 个硬编码遗物
