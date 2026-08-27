# P0 PRE-IMPLEMENTATION TRACE

> Phase: P0 实施前调用链追踪  
> 生成时间：2026-08-27  
> 目标：确认所有死亡入口、设计 Single Death Resolution Point、验证双重伤害

---

## A. Monster 全部死亡入口调用链

### A.1 伤害施加点（take_damage on Monster）— 22 处

| # | File:line | 函数 | 伤害来源 | 施加后行为 |
|---|-----------|------|---------|-----------|
| 1 | weapon_executor.cpp:97 | `_resolve_one()` | 近战武器 | **返回 WeaponAttackResult，不 erase** |
| 2 | weapon_executor.cpp:465 | `tick_projectiles()` | 投射物 | **返回结果，不 erase** |
| 3 | skill.cpp:249 | `SlashSkill::execute()` | 技能（斩击） | 累积到 total，不 erase |
| 4 | skill.cpp:275 | `SlashSkill::execute()` | 技能（重击波） | 同上 |
| 5 | skill.cpp:353 | `FireballSkill::execute()` | 技能（火球） | 同上 |
| 6 | skill.cpp:373 | `FireballSkill::execute()` | 技能（分裂火球） | 同上 |
| 7 | skill.cpp:558 | `IceNovaSkill::execute()` | 技能（冰新星） | 同上 |
| 8 | skill.cpp:569 | `IceNovaSkill::execute()` | 技能（暴风雪） | 同上 |
| 9 | skill.cpp:600 | `ChainLightningSkill::execute()` | 技能（连锁闪电） | 同上 |
| 10 | skill.cpp:644 | `ShadowStrikeSkill::execute()` | 技能（暗影斩） | 同上 |
| 11 | skill.cpp:675 | `BloodFrenzySkill::execute()` | 技能（血怒） | 同上 |
| 12 | combat_coordinator.cpp:28 | `player_attack()` | 近战（旧路径） | **erase，不触发 on_monster_killed** |
| 13 | combat_system.cpp:356 | `tick_buffs()` | DOT（毒/流血/灼烧） | 不 erase，委托 cleanup |
| 14 | ai.cpp:59 | `MonsterAI::update()` | 自爆（BOMBER） | 不 erase，委托 cleanup |
| 15 | arena_manager.cpp:107 | `ArenaManager::tick()` | 竞技场暗影墙 | 不 erase，委托 cleanup |
| 16 | game_scene.cpp:833 | `_process_arena_objects()` | 地刺 | 不 erase，委托 cleanup |
| 17 | game_scene.cpp:862 | `_process_arena_objects()` | 岩浆 | 不 erase，委托 cleanup |
| 18 | game_scene.cpp:891 | `_tick()` | 时停结束冲击波 | 不 erase，委托 cleanup |
| 19 | game_scene.cpp:1353 | `_explode_barrel()` | 炸弹桶 AOE | 不 erase，委托 cleanup |
| 20 | game_scene_combat.cpp:193 | `on_monster_killed()` 内 | thunder_core 遗物 AOE | 不 erase，委托下帧 cleanup |
| 21 | game_scene_combat.cpp:228 | `apply_pending_damage()` | 时停挂起伤害 | **erase + on_monster_killed** |
| 22 | combat_coordinator.cpp:240 | `apply_pending_damage()` | 时停挂起伤害（旧版） | **erase + on_monster_killed** |

### A.2 on_monster_killed 调用点 — 8 处

| # | File:line | 调用者 | 调用哪个实现 | 同处 erase? |
|---|-----------|--------|------------|------------|
| 1 | game_scene_combat.cpp:212 | `cleanup_dead_monsters()` | self | ✅ line 213 |
| 2 | game_scene_combat.cpp:233 | `apply_pending_damage()` | self | ✅ line 234 |
| 3 | combat_coordinator.cpp:229 | `cleanup_dead_monsters()` | self | ✅ line 230 |
| 4 | combat_coordinator.cpp:245 | `apply_pending_damage()` | self | ✅ line 246 |
| 5 | **game_scene.cpp:1006** | `_tick()` weapon specials | `_combat.on_monster_killed()` | **❌ 不 erase** |
| 6 | **game_scene.cpp:1012** | `_tick()` weapon projectiles | `_combat.on_monster_killed()` | **❌ 不 erase** |
| 7 | player_controller.cpp:599 | `_kill_target()` | `gs._on_monster_killed()` | ✅ line 602 |
| 8 | player_controller.cpp:634 | `use_skill()` cleanup | `gs._on_monster_killed()` | ✅ line 635 |

### A.3 Monster erase 点 — 7 处

| # | File:line | 调用者 | erase 前有 on_monster_killed? |
|---|-----------|--------|------------------------------|
| 1 | player_controller.cpp:602 | `_kill_target()` | ✅ line 599 |
| 2 | player_controller.cpp:635 | `use_skill()` | ✅ line 634 |
| 3 | game_scene_combat.cpp:213 | `cleanup_dead_monsters()` | ✅ line 212 |
| 4 | game_scene_combat.cpp:234 | `apply_pending_damage()` | ✅ line 233 |
| 5 | combat_coordinator.cpp:46 | `player_attack()` | **❌ 注释："不触发击杀逻辑，仅移除"** |
| 6 | combat_coordinator.cpp:230 | `cleanup_dead_monsters()` | ✅ line 229 |
| 7 | combat_coordinator.cpp:246 | `apply_pending_damage()` | ✅ line 245 |

### A.4 死亡来源汇总

| 死亡来源 | 伤害点 | on_monster_killed 入口 | erase 时机 | 重复风险 |
|---------|--------|----------------------|-----------|---------|
| 近战（_kill_target） | _resolve_one + apply_attack_damage | _kill_target 内 | 同帧 erase | ❌ 无 |
| 技能（use_skill） | skill.execute() | use_skill cleanup loop | 同帧 erase | ❌ 无 |
| **Weapon tick specials** | tick_specials | **game_scene.cpp:1006** | **下帧 cleanup** | **✅ 重复** |
| **Weapon tick projectiles** | tick_projectiles | **game_scene.cpp:1012** | **下帧 cleanup** | **✅ 重复** |
| DOT | tick_buffs | cleanup_dead_monsters | 同帧 erase | ❌ 无 |
| 时停挂起伤害 | apply_pending_damage | apply_pending_damage 内 | 同帧 erase | ❌ 无 |
| 竞技场地刺/岩浆 | arena hazard | cleanup_dead_monsters | 下帧 erase | ❌ 无（首次触发） |
| 炸弹桶 | _explode_barrel | cleanup_dead_monsters | 下帧 erase | ❌ 无（首次触发） |
| thunder_core 遗物 | on_monster_killed 内 | cleanup_dead_monsters | 下帧 erase | ❌ 无（首次触发） |

---

## B. Single Death Resolution Point 推荐位置

### B.1 现有架构分析

当前存在 **3 个并行的死亡处理实现**：

```
GameSceneCombat::on_monster_killed()    ← 主实现（完整：EventBus/XP/Loot/Relic/Stats）
CombatCoordinator::on_monster_killed()  ← 简化版（XP + leech_blade + battle_totem）
CombatCoordinator::cleanup_dead_monsters() ← 死代码（未被外部调用）
```

**推荐**：保留 `GameSceneCombat::on_monster_killed()` 作为 **唯一 Single Death Resolution Point**。

### B.2 推荐最终调用链

```
所有伤害来源
  ↓
take_damage() → is_alive = false
  ↓
[立即处理路径]（近战/技能/时停挂起伤害）
  ↓
GameSceneCombat::on_monster_killed(m)  ← Single Entry
  ├─ guard: if (m->kill_processed) return
  ├─ m->kill_processed = true
  ├─ EventBus: MONSTER_DIED / BOSS_DEAD
  ├─ run_stats.total_kills++
  ├─ XP + level-up
  ├─ 掉落（loot/boss reward）
  ├─ 遗物 on-kill 效果
  ├─ 楼层清理检测
  └─ 归档/统计
  ↓
erase (同处)

[延迟处理路径]（DOT/hazard/投射物/炸弹桶）
  ↓
is_alive = false，不立即处理
  ↓
_cleanup_dead_monsters() → cleanup_dead_monsters()
  ↓
GameSceneCombat::on_monster_killed(m)  ← 同一入口
  ├─ guard: if (m->kill_processed) return
  ├─ ... 同上 ...
  ↓
erase (同处)
```

### B.3 关键修改

| 修改点 | 内容 |
|--------|------|
| game_scene_combat.h | Monster 增加 `bool kill_processed = false` |
| game_scene_combat.cpp:27 | `on_monster_killed` 入口加 `if (!m \|\| m->kill_processed) return; m->kill_processed = true;` |
| game_scene.cpp:1006 | weapon specials 击杀后 **不调用** on_monster_killed，仅标记 → 委托下帧 cleanup |
| game_scene.cpp:1012 | weapon projectiles 击杀后 **不调用** on_monster_killed，仅标记 → 委托下帧 cleanup |
| game_scene_combat.cpp:212 | cleanup_dead_monsters 内的 on_monster_killed 调用保持不变（守卫兜底） |

### B.4 game_scene.cpp:1006/1012 修改方案

当前：
```cpp
// game_scene.cpp:1002-1013
for (auto& r : spec_results) {
    _boss.dmg_done += r.damage;
    // ... VFX ...
    if (r.is_killing_blow) _combat.on_monster_killed(r.target);  // ← 问题：不 erase
}
for (auto& r : proj_results) {
    _boss.dmg_done += r.damage;
    // ... VFX ...
    if (r.is_killing_blow) _combat.on_monster_killed(r.target);  // ← 问题：不 erase
}
```

修改为：
```cpp
for (auto& r : spec_results) {
    _boss.dmg_done += r.damage;
    // ... VFX ...
    // P0: 不在此处调用 on_monster_killed，委托下帧 cleanup_dead_monsters 统一处理
    // kill_processed guard 确保不重复
}
for (auto& r : proj_results) {
    _boss.dmg_done += r.damage;
    // ... VFX ...
    // P0: 同上
}
```

**效果**：weapon tick 击杀的怪物在下帧 `_cleanup_dead_monsters()` 时统一处理，`on_monster_killed` 只触发一次。

### B.5 对比：当前 vs 修改后

| 场景 | 当前行为 | 修改后行为 |
|------|---------|-----------|
| 近战击杀 | on_monster_killed + erase（正常） | 不变 |
| 技能击杀 | on_monster_killed + erase（正常） | 不变 |
| weapon tick 击杀 | on_monster_killed（第1次）→ 下帧 cleanup on_monster_killed（第2次） | 仅下帧 cleanup on_monster_killed（1次） |
| DOT 击杀 | 下帧 cleanup on_monster_killed（1次） | 不变 |
| 时停挂起击杀 | apply_pending_damage on_monster_killed + erase（正常） | 不变 |

---

## C. Double Damage 是否真实存在

### C.1 调用链证据

```
PlayerController::player_attack()                         // player_controller.cpp:308
  └─ _weapon_attack(gs, p)                                // player_controller.cpp:324
       ├─ WeaponExecutor::execute(...)                    // player_controller.cpp:421
       │    └─ _melee_normal(...)                         // weapon_executor.cpp:201
       │         └─ _resolve_one(p, h.target, ...)        // weapon_executor.cpp:362
       │              └─ m->combat.take_damage(ar.damage) // weapon_executor.cpp:97 ★ DAMAGE #1
       │
       └─ for (auto& r : results)                         // player_controller.cpp:555
            └─ _process_weapon_result(gs, p, r)           // player_controller.cpp:557
                 └─ CombatCoordinator::apply_attack_damage(r.target, r.damage, ...)
                      └─ target->combat.take_damage(dmg)  // combat_coordinator.cpp:53 ★ DAMAGE #2
```

### C.2 代码证据

**_resolve_one** (`weapon_executor.cpp:83-106`):
```cpp
int hp_before = m->combat.current_hp;
m->combat.take_damage(ar.damage);              // ★ 伤害已施加
ar.is_killing_blow = (!m->combat.is_alive && hp_before > 0);
// ... ElementResolver effects ...
return ar;  // ar.damage 仍携带伤害值
```

**apply_attack_damage** (`combat_coordinator.cpp:51-58`):
```cpp
void CombatCoordinator::apply_attack_damage(Monster* target, int dmg, ...) {
    target->combat.take_damage(dmg);           // ★ 同一 target 再次施加伤害
    // ... VFX ...
}
```

**_process_weapon_result** (`player_controller.cpp:395-412`):
```cpp
CombatCoordinator::apply_attack_damage(r.target, r.damage, ...);  // ★ 调用第二次 take_damage
if (r.is_killing_blow) _kill_target(gs, r.target);
```

### C.3 结论

**双重伤害 Bug 确认为真实。**

每次普通武器攻击（非 fist、非 special、非 projectile）对同一目标造成 **2x 预期伤害**。

### C.4 受影响范围

| 攻击路径 | 是否双重伤害 |
|---------|------------|
| 普通近战武器（sword/crossbow/staff/nunchaku 等） | **✅ 是** |
| tick_specials（nunchaku 连击、spear rapid） | ❌ 否（game_scene.cpp 直接处理，不走 _process_weapon_result） |
| tick_projectiles（crossbow bolt） | ❌ 否（同上） |
| Fist（无武器） | ❌ 否（走旧路径 apply_attack_damage 单次） |

### C.5 修复方案

**方案 A（推荐）**：从 `_resolve_one` 中移除 `take_damage`，让 `_process_weapon_result` 成为唯一伤害施加点。

修改 `_resolve_one`：
```cpp
// weapon_executor.cpp:97 — 移除 take_damage
int hp_before = m->combat.current_hp;  // 保留 HP 快照
// m->combat.take_damage(ar.damage);   // ← 删除此行
// is_killing_blow 改为由调用方在 take_damage 后设置
```

修改 `_process_weapon_result`：
```cpp
// player_controller.cpp:401-411
int hp_before = r.target->combat.current_hp;
CombatCoordinator::apply_attack_damage(r.target, r.damage, ...);  // 唯一伤害施加
bool actual_killing = (!r.target->combat.is_alive && hp_before > 0);
if (actual_killing) _kill_target(gs, r.target);
```

**方案 B**：从 `_process_weapon_result` 中移除 `apply_attack_damage`。

问题：会丢失 `apply_attack_damage` 中的 VFX/audio（hit flash + SFX）。

**推荐方案 A**。

### C.6 注意事项

- 移除 `_resolve_one` 的 `take_damage` 后，`ElementResolver::resolve()` 和 `ElementResolver::on_hit()` 需要在伤害施加前还是后执行？当前 `resolve()` 在 `take_damage` 前调用（line 95），`on_hit()` 在后调用（line 99）。改为方案 A 后，`resolve()` 仍在 `apply_attack_damage` 前（计算元素效果），`on_hit()` 需要移到 `apply_attack_damage` 后。
- `tick_specials` 和 `tick_projectiles` 的 `_resolve_one` 调用路径不经过 `_process_weapon_result`，所以移除 `_resolve_one` 的 `take_damage` 后，这两个路径也需要在调用方施加伤害。需确认 `tick_specials`/`tick_projectiles` 内部是否已有 `take_damage` 调用。

---

## D. M0.1A 两个遗物的参数读取链

### D.1 war_drum

**JSON** (`resources/relics.json:7`):
```json
{"id":"war_drum","name":"战鼓","short_name":"鼓","desc":"攻击力提升15%","rarity":"common","param":0.15,"hud_color":[200,60,60]}
```

**当前代码** (`combat_system.cpp:387`):
```cpp
if (player_has_relic(p, "war_drum"))    relic_sum += 0.10f; // hardcoded，JSON 说 0.15
```

**修复后** (`combat_system.cpp:387`):
```cpp
if (player_has_relic(p, "war_drum")) {
    const RelicDef* def = get_relic_def("war_drum");
    relic_sum += (def ? def->param : 0.15f);  // 从 JSON 读取，fallback 为设计值
}
```

**数据链**：
```
relics.json "param":0.15
  ↓ Registry 加载
  ↓ _parse_relic_obj → out.param = _read_float(p)
  ↓ g_relic_defs["war_drum"].param = 0.15f
  ↓ get_relic_def("war_drum")->param
  ↓ relic_sum += 0.15f
```

### D.2 blood_chalice

**JSON** (`resources/relics.json:21`):
```json
{"id":"blood_chalice","name":"血杯","short_name":"杯","desc":"生命越低攻击越高(最高+30%)","rarity":"rare","param":0.30,"hud_color":[220,40,40]}
```

**当前代码** (`combat_system.cpp:392-394`):
```cpp
if (player_has_relic(p, "blood_chalice")) {
    float hp_r = (float)p->combat.current_hp / get_effective_max_hp(p);
    relic_sum += (1.0f - hp_r) * 0.20f; // hardcoded cap 0.20，JSON 说 0.30
}
```

**修复后** (`combat_system.cpp:392-395`):
```cpp
if (player_has_relic(p, "blood_chalice")) {
    const RelicDef* def = get_relic_def("blood_chalice");
    float max_mult = def ? def->param : 0.30f;  // 从 JSON 读取
    float hp_r = (float)p->combat.current_hp / get_effective_max_hp(p);
    relic_sum += (1.0f - hp_r) * max_mult;
}
```

**数据链**：
```
relics.json "param":0.30
  ↓ Registry 加载
  ↓ _parse_relic_obj → out.param = _read_float(p)
  ↓ g_relic_defs["blood_chalice"].param = 0.30f
  ↓ get_relic_def("blood_chalice")->param
  ↓ max_mult = 0.30f
  ↓ relic_sum += (1.0 - hp%) * 0.30f
```

### D.3 影响评估

| 遗物 | 修改前 | 修改后 | 胜率影响 |
|------|--------|--------|---------|
| war_drum | ATK +10% | ATK +15% | 预计 +1-2% 胜率 |
| blood_chalice | 低血 ATK 最高 +20% | 低血 ATK 最高 +30% | 预计 +1-2% 胜率 |

需做 300 局模拟回归确认。

---

## E. 预计修改文件

### P0-A: On-Kill Exactly Once

| 文件 | 修改 |
|------|------|
| `src/game/scene/game_scene_combat.h` | Monster 增加 `bool kill_processed = false` |
| `src/game/scene/game_scene_combat.cpp:27` | on_monster_killed 加 kill_processed 守卫 |
| `src/game/scenes/game_scene.cpp:1006` | 移除 weapon specials 的 on_monster_killed 调用 |
| `src/game/scenes/game_scene.cpp:1012` | 移除 weapon projectiles 的 on_monster_killed 调用 |

### P0-B: Double Damage Fix

| 文件 | 修改 |
|------|------|
| `src/game/systems/weapon_executor.cpp:97` | _resolve_one 移除 take_damage，保留 HP 快照 |
| `src/game/entities/player_controller.cpp:401-411` | _process_weapon_result 在 apply_attack_damage 后设置 is_killing_blow |
| `src/game/systems/weapon_executor.cpp:95-100` | ElementResolver 调用时序调整 |

### M0.1A: 参数一致性

| 文件 | 修改 |
|------|------|
| `src/game/systems/combat_system.cpp:387` | war_drum hardcoded 0.10f → def->param |
| `src/game/systems/combat_system.cpp:392-394` | blood_chalice hardcoded 0.20f → def->param |
