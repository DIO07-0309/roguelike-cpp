# RELIC SYSTEM IMPLEMENTATION AUDIT

> Phase 0 + Phase 0.1：实施前核对 + 数据一致性审计  
> 生成时间：2026-08-27  
> 目标：确认当前代码真实状态，为"仿以撒圣遗物系统优化"提供实施基线

---

## A. 当前真实架构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        resources/relics.json                        │
│  60 条遗物定义：id/name/desc/rarity/param/param2/hud_color/tags    │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ Registry.register_module("relic")
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                  combat_system.cpp (g_relic_defs)                   │
│  static unordered_map<string, RelicDef>  ← 60 条 RelicDef          │
│  RelicDef { id, name, short_name, desc, rarity, param, param2,     │
│             hud_color_r/g/b, favorite_tags }                       │
│                                                                     │
│  全局查询 API：                                                      │
│  ├─ get_relic_def(id) → const RelicDef*                            │
│  ├─ get_all_relic_ids() → vector<string>                            │
│  ├─ player_has_relic(p, id) → bool  ← 唯一遗物查询函数              │
│  ├─ get_relic_display_name/short_name/hud_color                     │
│  └─ load_relic_defs / load_relic_defs_from_json                     │
└──────────────────────────────┬──────────────────────────────────────┘
                               │ player_has_relic() 硬编码散落
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    36+ 个 player_has_relic 调用点                    │
│                                                                     │
│  combat_system.cpp        (18处) — ATK/speed/HP/buff_tick 被动数值 │
│  combat_coordinator.cpp   (2处)  — on-kill 触发器                   │
│  game_scene_combat.cpp    (7处)  — on-kill 触发器 + boss pool       │
│  game_scene.cpp           (1处)  — floor enter 触发器               │
│  special_room.cpp         (8处)  — altar/chest/fountain/gambler     │
│  event_system.cpp         (7处)  — 过滤去重（不触发效果）            │
│  quest_manager.cpp        (1处)  — 任务奖励检查                      │
└─────────────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     RelicInstance (Player.relics)                    │
│  struct { string id; bool from_boss; }                              │
│  无 stack / level / runtime state / duration                        │
└─────────────────────────────────────────────────────────────────────┘
```

---

## B. 当前遗物数据流

### B.1 加载链路

```
resources/relics.json
  ↓ Registry.register_module("relic", load_relic_defs_from_json)
  ↓ main.cpp:200 → builtin_provider.cpp:12 → registry auto-load
  ↓ _parse_relic_obj (combat_system.cpp:475)
  ↓ _apply_relic_defaults (combat_system.cpp:542)
  ▼
g_relic_defs (static map, combat_system.cpp:471)
  ↓ get_relic_def(id) / get_all_relic_ids()
  ▼
  各消费点直接查询
```

### B.2 获得链路

```
事件/战斗掉落
  ↓ player_has_relic() 过滤去重
  ↓ player->relics.push_back({id, from_boss})
  ↓ g_relic_archive.mark_obtained()  ← 仅 2/10 条路径调用
  ↓ EventBus::emit(RELIC_GAIN)       ← 仅 1/10 条路径调用
  ▼
Player.relics vector (跨楼层: 仅 from_boss=true 保留)
```

### B.3 消费链路

```
player_has_relic(p, "xxx") → 遍历 p->relics 匹配 id
  ↓
if (match) { /* 硬编码效果 */ }
  ↓
无统一入口 / 无触发器引擎 / 无效果分发
```

### B.4 存档链路

```
SaveManager::save() → 不保存 relics (B13: Relic 不再跨层)
SaveManager::load() → 不读取 relics, 忽略旧档 rlc: 行
                      Boss relics 仅跨楼层, 不跨存档
```

---

## C. 所有遗物效果消费点

### C.1 战斗系统 (combat_system.cpp)

| 行号 | 遗物 ID | 效果类型 | 实现方式 |
|------|---------|---------|---------|
| 305 | plague_mask | 玩家毒伤 -1 | 硬编码 if |
| 341 | venom_fang | 怪物毒伤 +1 | 硬编码 if |
| 387 | war_drum | ATK ×1.10 | 硬编码 if |
| 388 | hunter_gloves | ATK ×1.08 | 硬编码 if |
| 389 | ancient_crown | ATK ×1.06 | 硬编码 if |
| 390 | dragon_heart | ATK ×1.10 | 硬编码 if |
| 391 | infinity_orb | ATK ×1.12 | 硬编码 if |
| 392 | blood_chalice | ATK ×(1+0.2×(1-HP%)) | 硬编码 if |
| 409 | hunters_eye | 速度 +10% | 硬编码 if |
| 414 | traveler_boots | 速度 +8% | 硬编码 if |
| 415 | ancient_crown | 速度 +5% | 硬编码 if |
| 416 | dragon_heart | 速度 +8% | 硬编码 if |
| 417 | infinity_orb | 速度 +10% | 硬编码 if |
| 439 | blood_charm | 最大HP +20 | 硬编码 if |
| 440 | iron_heart | 最大HP +10 | 硬编码 if |
| 441 | dragon_heart | 最大HP +30 | 硬编码 if |
| 442 | ancient_crown | 最大HP +8 | 硬编码 if |
| 443 | infinity_orb | 最大HP +25 | 硬编码 if |

### C.2 战斗协调器 (combat_coordinator.cpp)

| 行号 | 遗物 ID | 效果类型 | 实现方式 |
|------|---------|---------|---------|
| 203 | leech_blade | 击杀 20% 回 5HP | 硬编码 if |
| 213 | battle_totem | 击杀 15% 攻击强化 Buff | 硬编码 if |

### C.3 战斗场景 (game_scene_combat.cpp)

| 行号 | 遗物 ID | 效果类型 | 实现方式 |
|------|---------|---------|---------|
| 163 | leech_blade | 击杀 20% 回 5HP (重复!) | 硬编码 if |
| 170 | battle_totem | 击杀 15% 攻击强化 (重复!) | 硬编码 if |
| 175 | battle_medal | 击杀回 3HP | 硬编码 if |
| 178 | vampire_fang | 击杀回 8% maxHP | 硬编码 if |
| 183 | thunder_core | 击杀 30% AOE 雷击 | 硬编码 if |
| 199 | time_fragment | 击杀 5% 重置技能 CD | 硬编码 if |

### C.4 特殊房间 (special_room.cpp)

| 行号 | 遗物 ID | 效果类型 | 实现方式 |
|------|---------|---------|---------|
| 75 | sage_leaf | 祭坛回血 +10 | 硬编码 if |
| 77 | healing_herb | 祭坛回血 ×1.15 | 硬编码 if |
| 227 | merchant_coin | 宝箱遗物掉率 +15% | 硬编码 if |
| 236 | golden_dice | 宝箱多 1 件物品 | 硬编码 if |
| 242 | merchant_coin | 宝箱多 1 件物品 | 硬编码 if |
| 258 | sage_leaf | 泉水回血 +10 | 硬编码 if |
| 260 | healing_herb | 泉水回血 ×1.15 | 硬编码 if |
| 316 | golden_dice | 赌场胜率 60%→85% | 硬编码 if |

### C.5 楼层进入 (game_scene.cpp)

| 行号 | 遗物 ID | 效果类型 | 实现方式 |
|------|---------|---------|---------|
| 403 | soul_lantern | 进入楼层 +攻击Buff +10HP | 硬编码 if |

### C.6 事件系统 (event_system.cpp) — 仅去重过滤，无效果触发

7 个事件类型的遗物奖励池用 `player_has_relic` 过滤已持有遗物，不触发任何效果。

### C.7 任务系统 (quest_manager.cpp)

| 行号 | 来源 | 效果类型 | 实现方式 |
|------|------|---------|---------|
| 147 | 任务 JSON `relic_id` | 奖励遗物 | 数据驱动 |

### C.8 攻击进化 (attack_evolution.cpp)

| 行号 | 触发 | 效果类型 | 实现方式 |
|------|------|---------|---------|
| 16 | RELIC_GAIN 事件订阅 | 3 遗物→Lv2, 5 遗物→Lv3 | 事件驱动 |

### C.9 构建评分 (build_score.cpp)

| 行号 | 触发 | 效果类型 | 实现方式 |
|------|------|---------|---------|
| 27 | 遍历 player->relics | 遗物 tags 贡献 +2 BuildScore | 数据驱动 |
| 111 | relic_matches_build() | 遗物推荐权重 ×1.6 | 数据驱动 |

### C.10 归档系统 (relic_progression.cpp)

| 方法 | 调用点 | 功能 |
|------|--------|------|
| mark_obtained | game_scene_combat.cpp:138, special_room.cpp:188 | 记录获得次数/稀有度/熟练度 |
| collection_pct | game_scene.cpp:1527, gameplay_system_director.cpp:50, game_flow_director.cpp:84 | 传递收集率给结局 |
| mastery_level | game_renderer.cpp:427 | HUD 显示熟练度星标 |

---

## D. 设计文档与当前代码的差异

| 设计文档描述 | 代码实际状态 | 差异程度 |
|-------------|-------------|---------|
| "遗物效果散落在 10+ cpp" | ✅ 确认：7 个文件，36+ 个 player_has_relic 调用 | 完全一致 |
| "主要依赖硬编码 if" | ✅ 确认：36+ 个 if 分支，0 个数据驱动效果 | 完全一致 |
| "不存在统一遗物效果入口" | ✅ 确认：无 RelicEffectProcessor，无效果分发 | 完全一致 |
| "遗物之间无协同" | ✅ 确认：无 synergy 字段，无协同网络 | 完全一致 |
| "无套装/变形系统" | ✅ 确认：无 set 字段，无套装检测 | 完全一致 |
| "无 Build Identity 系统" | ⚠️ 部分存在：BuildScore + BuildTag + favorite_tags 已实现 | 可复用 |
| "60 个遗物" | ✅ 确认：relics.json 实际 60 条，get_all_relic_ids() 返回 60 个 | 完全一致 |
| "不改变核心手感" | ✅ 确认：ATK_COOLDOWN=0.5f constexpr，ComboState 无扩展 | 完全一致 |
| "遗物不修改攻击模式" | ✅ 确认：无弹道/穿透/分裂机制 | 完全一致 |

---

## E. 可直接复用的现有系统

| 系统 | 文件 | 可复用点 |
|------|------|---------|
| **EventBus** | core/event_bus.h | 45 事件类型 + subscribe/emit 机制 → 遗物触发器引擎直接复用 |
| **BuffSystem** | game/entities/buff.h | BuffTrigger 机制（定时/条件触发）→ 遗物效果可挂载为类 Buff 触发器 |
| **BuildScore** | game/build_score.h | BuildTag 枚举 + 权重累加 + 构建匹配 → 已有 favorite_tags |
| **BuildTheme** | game/build_theme.h | 热力图 + 最佳物品推荐 + 收集奖励 → 可直接扩展为套装系统 |
| **RelicArchiveSystem** | game/relic_progression.h | 归档/熟练度/收集奖励 → 可作为解锁系统基础 |
| **RelicDef.param/param2** | game/systems/combat_system.h | 已有数值参数槽位 → 效果数据化时可复用 |
| **Registry** | core/registry.h | 模块化 JSON 加载 → 遗物效果定义可走同一管线 |
| **GrowthCurve** | game/entities/buff.h | relic_scale 字段 → 已预留遗物稀有度缩放 |
| **SkillManager** | game/entities/skill.h | 技能冷却/消耗/Buff 挂载 → 机制改造时可参考 |
| **weapon_executor** | game/entities/weapon_executor.h | 近战/远程/法杖/拳套/魔法攻击分发 → 弹道扩展点 |

---

## F. 必须新增的最小能力

| 能力 | 说明 | 最小实现 |
|------|------|---------|
| **F1. RelicEffect 结构体** | 描述单个遗物效果（触发器+类型+参数） | 新增 struct，4-5 个字段 |
| **F2. RelicEffectProcessor** | 注册 EventBus 监听 → 遍历遗物 effects → 执行 | 新增 1 个类（header+cpp） |
| **F3. RelicEffect JSON 解析** | 解析 effects 数组字段 | 扩展 _parse_relic_obj |
| **F4. RelicInstance.runtime_data** | 运行时状态（套装计数器/协同标记） | 扩展 RelicInstance |
| **F5. SetDetector** | 检测套装成员数 → 激活/解除变形 | 可合并入 F2 |
| **F6. SynergyQuery** | 查询协同对是否同时持有 | 可合并入 F2 |
| **F7. player_has_relic → 数据化替代** | 36+ 个硬编码逐步替换为 EventBus 触发 | 分阶段迁移 |

---

## G. 风险点

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| **G1. 重复触发** | weapon tick 击杀时 on-kill 效果在同一文件内跨帧触发两次（kill moment + 下帧 cleanup） | 迁移时在 on_monster_killed 入口加 is_alive 守卫 |
| **G2. 归档缺口** | event_system/quest_manager 获得的遗物不写 g_relic_archive（8/10 获得路径缺失） | 迁移时统一写入归档 |
| **G3. RELIC_GAIN 缺口** | 仅 boss kill 发事件（1/10 获得路径），AttackEvolutionManager 只在 boss 时检查 | 迁移时所有遗物获得都 emit |
| **G4. ATK_COOLDOWN constexpr** | 0.5f 是编译期常量，遗物无法修改 | 需改为成员变量（影响小，仅 player.h） |
| **G5. 性能** | 遗物触发器引擎每帧 tick 遗物列表 → 60 遗物 × EventBus 事件 | 仅在事件触发时遍历，不在每帧 tick |
| **G6. 现有 build 破坏** | 修改 RelicInstance 结构体可能影响存档兼容 | B13 已不保存遗物，影响有限 |
| **G7. 测试覆盖** | 34 个测试中无遗物相关单元测试 | 迁移后需补充 |
| **G8. 模拟回归** | 修改遗物效果可能改变 300 局胜率 | 迁移前后做 A/B 对比回归 |
| **G9. Ghost 遗物** | 37/60 遗物无任何 C++ 实现，玩家可捡到但什么都不做 | M1A 期间可选择实现或从掉落池移除 |
| **G10. JSON/代码值不一致** | war_drum JSON param=0.15 代码用 0.10；blood_chalice JSON param=0.30 代码 cap 0.20 | 迁移时以 JSON param 为准 |
| **G11. CombatCoordinator 死代码** | CombatCoordinator::on_monster_killed/cleanup_dead_monsters/apply_pending_damage 均为不可达代码 | 迁移时可安全清理 |

---

## H. 推荐的 Milestone 拆分

### M1: 遗物效果数据化（消除硬编码）

**目标**：36+ 个 player_has_relic 硬编码 → RelicEffectProcessor 统一分发

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1.1 | RelicEffect 结构体定义 | 新增 relic_effect.h |
| 1.2 | RelicDef 增加 effects 字段 | combat_stats.h / combat_system.h |
| 1.3 | JSON 解析 effects 数组 | combat_system.cpp _parse_relic_obj |
| 1.4 | RelicEffectProcessor 核心（EventBus 注册+分发） | 新增 relic_effect_processor.h/cpp |
| 1.5 | 逐个替换硬编码（分批，每批 ~5 个遗物） | combat_system.cpp, game_scene_combat.cpp 等 |
| 1.6 | 删除 player_has_relic 函数（仅保留查询 API 用于去重） | combat_system.h/cpp |
| 1.7 | 补充 RELIC_GAIN 事件发射（所有获得路径） | event_system.cpp, special_room.cpp, quest_manager.cpp |
| 1.8 | 补充 g_relic_archive.mark_obtained（所有获得路径） | 同上 |

**验证**：300 局模拟胜率不变，34 个测试通过

### M2: 核心机制可修改

**目标**：遗物能改变攻速/连击段数/弹道形态

| 步骤 | 内容 | 文件 |
|------|------|------|
| 2.1 | ATTACK_COOLDOWN 改为成员变量 | player.h |
| 2.2 | ComboState 增加 max_combo | player.h |
| 2.3 | WeaponExecutor 支持遗物修饰符 | weapon_executor.h/cpp |
| 2.4 | 效果类型增加 modify_mechanic | relic_effect.h |
| 2.5 | 添加 3-5 个机制修改遗物（攻速/穿透/连击） | relics.json |

**验证**：新增遗物可改变战斗手感，模拟胜率仍在 6-10%

### M3: 套装系统

**目标**：8 套装 × 3 成员 → 变形效果

| 步骤 | 内容 | 文件 |
|------|------|------|
| 3.1 | RelicInstance 增加 set 字段 | combat_stats.h |
| 3.2 | RelicEffectProcessor 增加 SetDetector | relic_effect_processor.h |
| 3.3 | 为现有遗物添加 set 标签 | relics.json |
| 3.4 | 定义 8 套装变形效果 | 新增 relic_sets.json 或扩展 |
| 3.5 | 变形效果 UI 提示 | game_renderer.cpp |

**验证**：套装变形可在战斗中触发，不影响现有 build

### M4: 协同网络

**目标**：20 组高感知度协同

| 步骤 | 内容 | 文件 |
|------|------|------|
| 4.1 | RelicInstance 增加 synergy 向量 | combat_stats.h |
| 4.2 | 协同查询逻辑 | relic_effect_processor.cpp |
| 4.3 | 为现有遗物添加 10-15 组协同 | relics.json |
| 4.4 | 协同激活 UI 提示 | game_renderer.cpp |

**验证**：协同可在战斗中触发，不影响现有 build

---

## I. 预计修改文件

| 文件 | 修改类型 | 阶段 |
|------|---------|------|
| `src/game/entities/combat_stats.h` | RelicInstance 扩展 | M1-M3 |
| `src/game/systems/combat_system.h` | RelicDef 扩展 + 删除 player_has_relic | M1 |
| `src/game/systems/combat_system.cpp` | 替换 18 处硬编码 + JSON 解析扩展 | M1 |
| `src/game/systems/combat_coordinator.cpp` | 替换 2 处硬编码 | M1 |
| `src/game/scene/game_scene_combat.cpp` | 替换 7 处硬编码 + 去重 | M1 |
| `src/game/scene/game_scene.cpp` | 替换 1 处硬编码 | M1 |
| `src/game/world/special_room.cpp` | 补充归档写入 + 替换 8 处硬编码 | M1 |
| `src/game/world/event_system.cpp` | 补充 RELIC_GAIN + 归档写入 | M1 |
| `src/game/world/quest_manager.cpp` | 补充 RELIC_GAIN + 归档写入 | M1 |
| `src/game/entities/player.h` | ATTACK_COOLDOWN 改成员变量 + ComboState 扩展 | M2 |
| `src/game/entities/weapon_executor.h/cpp` | 遗物修饰符支持 | M2 |
| `src/game/systems/game_renderer.cpp` | 套装/协同 UI | M3-M4 |
| **新增** `src/game/systems/relic_effect_processor.h` | 效果处理器 | M1 |
| **新增** `src/game/systems/relic_effect_processor.cpp` | 效果处理器 | M1 |
| `resources/relics.json` | effects/synergy/set 字段 | M1-M4 |

---

> **审计结论**：设计文档与代码实际状态高度一致。当前遗物系统确实存在"硬编码散落/无统一入口/无协同/无套装/不改机制"五个核心问题。可复用的 EventBus + BuildScore + RelicArchive 提供了良好的扩展基础。建议从 M1A（基础设施）开始实施，预计 3 天可完成。

---

## J. Phase 0.1 — Relic Data Consistency Audit

> 生成时间：2026-08-27  
> 目标：确认遗物数据一致性、重复触发、Ghost 遗物

### J.1 数量核实

| 指标 | 结果 |
|------|------|
| `relics.json` 实际条目数 | **60** |
| `get_all_relic_ids()` 返回数量 | **60** |
| 重复 ID | **0** |
| 代码引用但 JSON 缺失的 relic ID | **0** |

**结论**：JSON 与代码完全一致，60 条遗物全部存在。第一次审计的 "48 条" 为错误计数。

### J.2 Ghost 遗物清单（37/60 无 C++ 实现）

以下 37 个遗物存在于 `relics.json` 但代码中**零引用**——玩家可以捡到但什么都不做：

| 分类 | 遗物 ID | 数量 |
|------|---------|------|
| **冰系** | frozen_crystal, frozen_heart, glacier_shard, winter_crown, absolute_zero | 5 |
| **雷系** | thunder_orb, storm_caller, lightning_rod, divine_storm | 4 |
| **流血** | crimson_blade, blood_pool, sacrificial_dagger, hemoplague | 4 |
| **召唤** | soul_chain, necromancer_tome, swarm_queen, spirit_army | 4 |
| **防御** | bastion_shield, unyielding, iron_determination, phoenix_rebirth | 4 |
| **暗影** | shadow_cloak, assassin_mark, night_veil | 3 |
| **时间** | chrono_stone, paradox_orb | 2 |
| **火系** | ignition_ring, inferno_core | 2 |
| **毒系** | venom_vein, plague_heart | 2 |
| **通用** | iron_ring, tiny_shield, magic_quill, emerald_heart, phoenix_feather, chaos_dice, god_hand | 7 |

**已实现的 23 个遗物**：

| 遗物 ID | 效果概述 | 读取 JSON param |
|---------|---------|----------------|
| blood_charm | 最大HP +20 | ❌ hardcoded |
| venom_fang | 怪物毒伤 +1 | ❌ hardcoded |
| golden_dice | 宝箱多 1 件 + 赌场胜率 | ❌ hardcoded |
| hunters_eye | 速度 +10% | ✅ `def->param` |
| leech_blade | 击杀 20% 回 5HP | ✅ `def->param` + `def->param2` |
| war_drum | ATK +10% | ❌ hardcoded（**JSON param=0.15，代码用 0.10**） |
| battle_totem | 击杀 15% 攻击强化 | ✅ `def->param` |
| iron_heart | 最大HP +10 | ❌ hardcoded |
| sage_leaf | 祭坛/泉水回血 +10 | ❌ hardcoded |
| merchant_coin | 宝箱遗物掉率 +15% | ❌ hardcoded |
| plague_mask | 玩家毒伤 -1 | ❌ hardcoded |
| hunter_gloves | ATK +8% | ❌ hardcoded |
| traveler_boots | 速度 +8% | ❌ hardcoded |
| healing_herb | 治疗效果 +15% | ❌ hardcoded |
| battle_medal | 击杀回 3HP | ❌ hardcoded |
| blood_chalice | 低血 ATK 加成（最高 20%） | ❌ hardcoded（**JSON param=0.30，代码 cap 0.20**） |
| vampire_fang | 击杀回 8% maxHP | ❌ hardcoded |
| thunder_core | 击杀 30% AOE 雷击 | ❌ hardcoded |
| time_fragment | 击杀 5% 重置技能 CD | ❌ hardcoded |
| soul_lantern | 进入楼层 +攻击Buff +10HP | ❌ hardcoded |
| ancient_crown | ATK+6% 速度+5% HP+8 | ❌ hardcoded |
| infinity_orb | ATK+12% 速度+10% HP+25 | ❌ hardcoded |
| dragon_heart | ATK+10% 速度+8% HP+30 | ❌ hardcoded |

### J.3 重复触发 Bug 确认

**结论**：两个文件之间的重复是**误判**。`CombatCoordinator::on_monster_killed()` 是**不可达死代码**。

实际重复发生在 `game_scene_combat.cpp` **内部跨帧**：

```
帧 N: WeaponExecutor::tick_specials() 击杀怪物
  → game_scene.cpp:1006 调用 _combat.on_monster_killed(target)  ← 第 1 次触发
  → 怪物未从 monsters 列表移除（is_alive=false 但仍存在）

帧 N+1: _cleanup_dead_monsters()
  → game_scene_combat.cpp:211 发现 is_alive=false
  → game_scene_combat.cpp:212 再次调用 on_monster_killed(it->get())  ← 第 2 次触发
  → game_scene_combat.cpp:213 终于 erase 怪物
```

**触发条件**：仅当怪物被 `WeaponExecutor::tick_specials()` 或 `tick_projectiles()` 击杀时。近战/技能击杀会立即 erase，不触发此 bug。

**受影响的遗物效果**：
- leech_blade：两次独立 RNG → 实际回血概率翻倍
- battle_totem：两次独立 RNG → 实际触发概率翻倍
- battle_medal：保证触发两次 → 实际回血 6HP（应为 3HP）
- vampire_fang：保证触发两次 → 实际回血 16%（应为 8%）
- thunder_core：两次独立 30% RNG
- time_fragment：两次独立 5% RNG

### J.4 JSON vs 代码值不一致

| 遗物 | JSON param | 代码 hardcoded | 差异 | 影响 |
|------|-----------|---------------|------|------|
| war_drum | 0.15 (15%) | 0.10f (10%) | 代码偏低 5% | 玩家实际 ATK 加成低于描述 |
| blood_chalice | 0.30 (30%) | 0.20f (20% cap) | 代码偏低 10% | 低血 ATK 加成上限低于描述 |

### J.5 CombatCoordinator 死代码确认

以下方法定义了但**从未被外部调用**：

| 方法 | 状态 |
|------|------|
| `CombatCoordinator::on_monster_killed()` | ❌ 死代码 |
| `CombatCoordinator::cleanup_dead_monsters()` | ❌ 死代码 |
| `CombatCoordinator::apply_pending_damage()` | ❌ 死代码 |
| `CombatCoordinator::player_attack()` | ❌ 死代码（仅定义，未调用） |

**注意**：`CombatCoordinator::apply_attack_damage()` 和 `CombatCoordinator::use_skill()` 仍被 `player_controller.cpp` 调用，不是死代码。

### J.6 归档写入缺口

| 获得路径 | 调用 mark_obtained | 调用 RELIC_GAIN emit |
|---------|-------------------|---------------------|
| boss kill (game_scene_combat.cpp) | ✅ | ✅ |
| chest/shrine (special_room.cpp) | ✅ | ❌ |
| event_system.cpp (7 种事件) | ❌ | ❌ |
| quest_manager.cpp | ❌ | ❌ |

**结论**：10 条获得路径中，仅 2 条写归档，仅 1 条发 RELIC_GAIN 事件。
