# RELIC SYSTEM REVISED IMPLEMENTATION PLAN

> Phase 0.1 Audit 后修订  
> 生成时间：2026-08-27  
> 原则：先修 Bug → 数据一致性 → 基础设施 → 逐类迁移，禁止一次性重构

---

## A. P0 Bug 修复方案

### A.1 On-Kill Exactly-Once 语义

**问题**：`on_monster_killed()` 可被同一怪物触发两次

**根因**：`game_scene.cpp:1006/1012`（weapon tick kills）调用 `on_monster_killed()` 后**未 erase 怪物**，下一帧 `_cleanup_dead_monsters()` 再次触发。

**受影响路径**：

| 击杀路径 | on_monster_killed | erase 怪物 | 是否重复 |
|---------|-------------------|-----------|---------|
| `_kill_target()` 近战 | ✅ | ✅ 同帧 | ❌ |
| `use_skill()` 技能 | ✅ | ✅ 同帧 | ❌ |
| `apply_pending_damage()` 时停伤害 | ✅ | ✅ 同帧 | ❌ |
| **tick_specials() 特殊攻击** | ✅ | ❌ 下帧 | **✅ 重复** |
| **tick_projectiles() 投射物** | ✅ | ❌ 下帧 | **✅ 重复** |
| `cleanup_dead_monsters()` buff DOT | ✅ | ✅ 同帧 | ❌ |

**修复方案**：不加 `is_alive` 守卫（会误杀 buff DOT 击杀），采用**标记已处理**方案：

```cpp
// game_scene_combat.h — RelicInstance 不变，Monster 增加标记
struct Monster {
    // ...existing fields...
    bool kill_processed = false;  // P0: 防止 on_monster_killed 重复触发
};
```

```cpp
// game_scene_combat.cpp — on_monster_killed 入口加守卫
void GameSceneCombat::on_monster_killed(Monster* m) {
    if (!m || m->kill_processed) return;  // P0: exactly-once
    m->kill_processed = true;
    // ... 原有逻辑不变 ...
}
```

```cpp
// game_scene_combat.cpp — cleanup_dead_monsters 无需改动（守卫在 on_monster_killed 内）
void GameSceneCombat::cleanup_dead_monsters() {
    auto it = _s.monsters.begin();
    while (it != _s.monsters.end()) {
        if (!(*it)->combat.is_alive) {
            on_monster_killed(it->get());  // 守卫确保不重复
            it = _s.monsters.erase(it);
        } else ++it;
    }
    // ...
}
```

**验证**：
- 近战/技能击杀：kill_processed 在 `_kill_target`/`use_skill` 中设为 true → cleanup 不重复
- weapon tick 击杀：第 1 次触发设 kill_processed=true → 第 2 次 cleanup 被守卫拦截
- buff DOT 击杀：`cleanup_dead_monsters` 是唯一路径 → 正常触发一次

### A.2 额外发现：双重伤害 Bug

**问题**：`_resolve_one()` 已调用 `take_damage()`，`_process_weapon_result()` 又调用 `apply_attack_damage()` 再次 `take_damage()` → 每次普通攻击造成 2x 伤害。

**修复方案**：在 `_process_weapon_result` 中跳过已由 `_resolve_one` 处理的伤害：

```cpp
// player_controller.cpp — _process_weapon_result
void PlayerController::_process_weapon_result(GameScene& gs, Player& p,
    const WeaponAttackResult& r)
{
    if (gs.time_stop_remaining > 0) {
        gs.pending_damage.emplace_back(r.target, r.damage);
        return;
    }
    // P0: _resolve_one 已调用 take_damage，此处不再重复
    // CombatCoordinator::apply_attack_damage(r.target, r.damage, ...);
    if (r.is_killing_blow) _kill_target(gs, r.target);
}
```

**注意**：需确认 `_resolve_one` 的 `take_damage` 返回值是否被正确用于判断 `is_killing_blow`。如果 `is_killing_blow` 是在 `_resolve_one` 中根据 `take_damage` 结果设置的，则此处跳过 `apply_attack_damage` 是安全的。

### A.3 修复优先级

| Bug | 修复复杂度 | 影响范围 | 优先级 |
|-----|-----------|---------|--------|
| On-Kill 重复触发 | 低（2 行改动） | 所有 on-kill 遗物 + XP + 掉落 + 统计 | **P0** |
| 双重伤害 | 中（需确认 _resolve_one 逻辑） | 所有普通攻击伤害 | **P0** |

---

## B. Data Driven 参数一致性方案

### B.1 当前不一致

| 遗物 | JSON param | 代码 hardcoded | 差异 |
|------|-----------|---------------|------|
| war_drum | 0.15 (15%) | 0.10f (10%) | 代码偏低 5% |
| blood_chalice | 0.30 (30%) | 0.20f (20% cap) | 代码偏低 10% |

### B.2 方案：JSON 为 Single Source of Truth

**原则**：所有已实现的 23 个遗物，效果参数统一从 `RelicDef.param` / `RelicDef.param2` 读取，不再 hardcoded。

**步骤**：
1. 逐个修改 23 个已实现遗物的 C++ 代码，将 hardcoded 值替换为 `def->param` / `def->param2`
2. 以 JSON 描述为准（war_drum=15%, blood_chalice=30%）
3. 迁移后做 300 局模拟回归，确认胜率仍在 6-10%

**注意**：blood_chalice 的 param=0.30 是上限值，代码中 cap 逻辑保留，但数值从 JSON 读取。

---

## C. 37 个 Ghost Relics 的 Effect Pattern 分类

### C.1 分类结果

| Effect Pattern | 数量 | 遗物 ID |
|---------------|------|---------|
| **STAT_MODIFIER** | 10 | iron_ring, magic_quill, glacier_shard, lightning_rod, crimson_blade, bastion_shield, iron_determination, ignition_ring, venom_vein, chrono_stone |
| **CONDITIONAL** | 6 | winter_crown, absolute_zero, divine_storm, assassin_mark, night_veil, paradox_orb |
| **ON_KILL** | 4 | thunder_orb, blood_pool, necromancer_tome, inferno_core |
| **ON_HIT** | 3 | frozen_crystal, frozen_heart, sacrificial_dagger |
| **ON_DEATH** | 3 | phoenix_feather, god_hand, phoenix_rebirth |
| **ON_DAMAGE_TAKEN** | 2 | tiny_shield, unyielding |
| **ON_FLOOR_ENTER** | 2 | shadow_cloak, spirit_army |
| **PASSIVE_TIMER** | 2 | emerald_heart, storm_caller |
| **SUMMON** | 2 | soul_chain, swarm_queen |
| **ON_SKILL_CAST** | 1 | chaos_dice |
| **AURA** | 1 | plague_heart |
| **STATUS_APPLICATION** | 1 | hemoplague |
| **合计** | **37** | |

### C.2 分析

- **STAT_MODIFIER 占 27%**：最简单的模式，参数化即可，无新触发器
- **CONDITIONAL 占 16%**：需要条件检测器，但条件类型有限（目标状态/自身状态/事件后置）
- **ON_KILL / ON_HIT / ON_DEATH 合计 24%**：可复用 EventBus 现有事件
- **稀有模式**（AURA / STATUS_APPLICATION / ON_SKILL_CAST）各仅 1 个，可特殊处理

---

## D. 最小通用 Effect Handler 集合

### D.1 Handler 列表（12 个）

| Handler | 覆盖 Pattern | 覆盖遗物数 | 复杂度 |
|---------|-------------|-----------|--------|
| **H1. StatModifierHandler** | STAT_MODIFIER | 10 | 低 |
| **H2. OnKillHandler** | ON_KILL | 4 + 现有 6 | 低 |
| **H3. OnHitHandler** | ON_HIT | 3 | 低 |
| **H4. OnDeathHandler** | ON_DEATH | 3 | 中 |
| **H5. OnDamageTakenHandler** | ON_DAMAGE_TAKEN | 2 | 低 |
| **H6. OnFloorEnterHandler** | ON_FLOOR_ENTER | 2 + 现有 1 | 低 |
| **H7. PassiveTimerHandler** | PASSIVE_TIMER | 2 | 低 |
| **H8. OnSkillCastHandler** | ON_SKILL_CAST | 1 | 低 |
| **H9. SummonHandler** | SUMMON | 2 | 高 |
| **H10. AuraHandler** | AURA | 1 | 中 |
| **H11. StatusApplicationHandler** | STATUS_APPLICATION | 1 | 中 |
| **H12. ConditionalHandler** | CONDITIONAL | 6 | 高 |

### D.2 覆盖率

- 12 个 Handler 覆盖全部 37 个 Ghost Relics
- 其中 H1 (StatModifier) 单独覆盖 27%
- H2 (OnKill) 同时覆盖现有 6 个 on-kill 遗物 + 4 个 ghost
- H6 (OnFloorEnter) 同时覆盖现有 soul_lantern + 2 个 ghost

### D.3 分阶段实施

**M1A 只实现 5 个 Handler**（覆盖 22/37 = 59% ghost + 全部现有 23 个遗物）：
- H1 StatModifierHandler（10 ghost + 现有 ATK/SPD/HP 修改）
- H2 OnKillHandler（4 ghost + 现有 6 on-kill）
- H3 OnHitHandler（3 ghost）
- H6 OnFloorEnterHandler（2 ghost + 现有 1）
- H5 OnDamageTakenHandler（2 ghost）

**M1B 实现剩余 7 个 Handler**：
- H4 OnDeathHandler, H7 PassiveTimerHandler, H8 OnSkillCastHandler
- H9-H12 特殊 Handler

---

## E. 新旧系统如何共存

### E.1 双轨运行策略

```
现有系统（保留）                    新系统（新增）
player_has_relic() 查询       ←→   RelicEffectProcessor 触发
硬编码 if 分支                 ←→   EventBus 事件 → Handler 执行
RelicDef.param/param2         ←→   RelicDef.effects[] 数组
```

### E.2 共存规则

1. **player_has_relic 保留**：去重过滤、BuildScore 查询、归档系统仍使用
2. **旧硬编码不删除**：M1A 期间新旧系统并行，通过 `#ifdef RELIC_V2` 或运行时开关控制
3. **新遗物只走新系统**：Ghost Relics 实现时只写 JSON effects，不加硬编码
4. **现有 23 个遗物逐步迁移**：M1B 开始逐个将硬编码替换为 JSON effects
5. **开关机制**：`RelicEffectProcessor::set_enabled(bool)` — 关闭时退化为纯旧系统

### E.3 数据流（新旧并存）

```
resources/relics.json
  ↓ Registry 加载
  ↓
g_relic_defs (RelicDef + effects[])
  ↓
  ├── 旧路径: player_has_relic() → 硬编码 if（保留，逐步废弃）
  │
  └── 新路径: EventBus 事件 → RelicEffectProcessor
        ↓ 遍历 player->relics
        ↓ 匹配 trigger → 查找 effects[]
        ↓ 执行对应 Handler
        ↓
      效果应用
```

---

## F. 推荐 Milestone

### P0 / M0：Bug 修复（1 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| M0.1 | Monster 增加 kill_processed 标记 | game_scene_combat.h |
| M0.2 | on_monster_killed 加 exactly-once 守卫 | game_scene_combat.cpp |
| M0.3 | 确认双重伤害 Bug 并修复 | player_controller.cpp |
| M0.4 | 300 局模拟回归 | — |

**验证**：胜率不变，on-kill 遗物效果不重复

### P0 / M0.1：参数一致性（0.5 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| M0.5 | 23 个已实现遗物 hardcoded → def->param | combat_system.cpp, game_scene_combat.cpp 等 |
| M0.6 | war_drum/blood_chalice 数值修正 | 同上 |
| M0.7 | 300 局模拟回归 | — |

**验证**：war_drum ATK 从 10% → 15%，blood_chalice cap 从 20% → 30%，胜率仍在 6-10%

### M1A：RelicEffect Foundation（3 天）

| 步骤 | 内容 | 文件 |
|------|------|------|
| M1A.1 | RelicEffect 结构体（trigger + type + param） | 新增 relic_effect.h |
| M1A.2 | RelicDef 增加 effects 字段 | combat_system.h |
| M1A.3 | JSON 解析 effects 数组 | combat_system.cpp |
| M1A.4 | RelicEffectProcessor 核心 | 新增 relic_effect_processor.h/cpp |
| M1A.5 | 5 个 Handler（H1/H2/H3/H5/H6） | relic_effect_processor.cpp |
| M1A.6 | 5 个低风险 Ghost Relics 完整实现 | relics.json + 验证 |
| M1A.7 | EventBus RELIC_GAIN 补全（所有获得路径） | event_system.cpp, special_room.cpp |
| M1A.8 | RelicArchive 补全（所有获得路径） | 同上 |

**验证**：5 个 Ghost Relics 可在游戏中生效，34 个测试通过

### M1B：首批 Ghost Relics 实现（2 天）

从 37 个 Ghost 中选择 **5-8 个**实现，覆盖多种 Pattern：

| 遗物 | Pattern | 选择理由 |
|------|---------|---------|
| iron_ring | STAT_MODIFIER | 最简单，验证 H1 |
| frozen_crystal | ON_HIT | 验证 H3 |
| tiny_shield | ON_DAMAGE_TAKEN | 验证 H5 |
| emerald_heart | PASSIVE_TIMER | 验证 H7 |
| phoenix_feather | ON_DEATH | 高感知度，验证 H4 |
| thunder_orb | ON_KILL | 与现有 thunder_core 对比 |

**验证**：新遗物可在游戏中生效，模拟胜率仍 6-10%

### M1C：旧硬编码迁移（3 天）

逐类迁移现有 23 个遗物的硬编码 → JSON effects：

| 批次 | Pattern | 遗物 |
|------|---------|------|
| 第一批 | ON_KILL | leech_blade, battle_totem, battle_medal, vampire_fang, thunder_core, time_fragment |
| 第二批 | ON_FLOOR_ENTER | soul_lantern |
| 第三批 | 特殊房间 | sage_leaf, healing_herb, merchant_coin, golden_dice |
| 第四批 | STAT_MODIFIER | 血杯/战鼓/猎手手套/旅者之靴 等核心属性 |

**验证**：删除所有 player_has_relic 硬编码后，模拟胜率不变

### M2+：后续阶段

保持原设计不变（M2 核心机制改造 → M3 套装 → M4 协同），但所有新功能只走 RelicEffectProcessor。

---

## G. 每个 Milestone 预计修改文件

### P0 / M0

| 文件 | 修改 |
|------|------|
| `src/game/scene/game_scene_combat.h` | Monster 增加 kill_processed |
| `src/game/scene/game_scene_combat.cpp` | on_monster_killed 加守卫 |
| `src/game/entities/player_controller.cpp` | _process_weapon_result 去重伤害 |

### P0 / M0.1

| 文件 | 修改 |
|------|------|
| `src/game/systems/combat_system.cpp` | 18 处 hardcoded → def->param |
| `src/game/scene/game_scene_combat.cpp` | 6 处 hardcoded → def->param |
| `src/game/world/special_room.cpp` | 8 处 hardcoded → def->param |
| `src/game/scene/game_scene.cpp` | 1 处 hardcoded → def->param |

### M1A

| 文件 | 修改 |
|------|------|
| **新增** `src/game/systems/relic_effect.h` | RelicEffect 结构体 |
| **新增** `src/game/systems/relic_effect_processor.h` | Processor 声明 |
| **新增** `src/game/systems/relic_effect_processor.cpp` | Processor + 5 Handler |
| `src/game/systems/combat_system.h` | RelicDef 增加 effects |
| `src/game/systems/combat_system.cpp` | JSON 解析 effects |
| `src/game/world/event_system.cpp` | RELIC_GAIN 补全 |
| `src/game/world/special_room.cpp` | RELIC_GAIN + 归档补全 |
| `src/game/world/quest_manager.cpp` | RELIC_GAIN + 归档补全 |
| `resources/relics.json` | 5 个 Ghost 增加 effects 字段 |
| `src/game/scene/game_scene.cpp` | 注册 RelicEffectProcessor |

### M1B

| 文件 | 修改 |
|------|------|
| `resources/relics.json` | 5-8 个 Ghost 增加 effects 字段 |
| `src/game/systems/relic_effect_processor.cpp` | 可能新增 Handler |

### M1C

| 文件 | 修改 |
|------|------|
| `src/game/systems/combat_system.cpp` | 删除 18 处 hardcoded |
| `src/game/systems/combat_coordinator.cpp` | 删除 2 处 dead code |
| `src/game/scene/game_scene_combat.cpp` | 删除 7 处 hardcoded |
| `src/game/scene/game_scene.cpp` | 删除 1 处 hardcoded |
| `src/game/world/special_room.cpp` | 删除 8 处 hardcoded |
| `resources/relics.json` | 23 个遗物增加 effects 字段 |

---

## H. 风险分析

| 风险 | 影响 | 缓解 |
|------|------|------|
| **H1. M0 修复引入新 Bug** | kill_processed 标记可能影响 boss 击杀流程 | 测试覆盖 boss 击杀路径 |
| **H2. 双重伤害修复影响 DPS** | 当前 2x 伤害可能是"预期"的隐性平衡 | 修复后做 300 局模拟，胜率可能下降 |
| **H3. param 一致性影响平衡** | war_drum 10%→15% 可能提高胜率 | 模拟回归，必要时调整其他数值 |
| **H4. RelicEffectProcessor 性能** | 60 遗物 × EventBus 事件 | 仅事件触发时遍历，不在每帧 tick |
| **H5. 新旧系统并存复杂度** | 双轨运行增加维护成本 | M1C 完成后删除旧硬编码 |
| **H6. Ghost Relics 实现后平衡** | 37 个新效果可能打破现有生态 | 分批实现，每批模拟回归 |
| **H7. 测试覆盖不足** | 34 个测试无遗物相关 | M1A 后补充单元测试 |
| **H8. 存档兼容** | kill_processed 标记不存档（B13） | 无影响 |

---

> **核心原则**：P0 Bug 修复优先于一切。On-Kill exactly-once 是语义问题，不是"删一个遗物效果"就能规避的。修完 Bug 再建基础设施。
