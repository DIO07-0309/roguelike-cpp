# Architecture Diagram v1

> Roguelike C++ (Raylib 5.0, C++17)
> 版本: G3.5 Final
> 日期: 2026-07-17
> 状态: Architecture Frozen — 进入 G4 Mod Support 阶段

---

## 一、整体分层架构

```
┌──────────────────────────────────────────────────────────────┐
│                     RESOURCES (10 JSON)                      │
│  buffs.json · relics.json · enemies.json · bosses.json      │
│  dialogues.json · quests.json · endings.json                │
│  meta_nodes.json · skills.json · items.json                 │
│                      156 配置条目                              │
└──────────────────────────┬───────────────────────────────────┘
                           │  load_*_defs()
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                    DATA REGISTRY (12 modules)                 │
│  src/data/   BuffDef  RelicDef  EnemyDef  BossDef           │
│              DialogueDef  QuestDef  EndingDef                │
│              MetaNodeDef  SkillDef  ItemDef                  │
│              RuleChainDef  AttackEvolution                   │
│  统一 API:  load / get / get_all / is_loaded                 │
└──────────────────────────┬───────────────────────────────────┘
                           │  get_*_def()
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                 FACTORY + MANAGER LAYER                       │
│  spawn_monster()  boss_factory_create()  skill_factory_create│
│  generate_random_item()  SkillManager  QuestManager          │
│  AttackEvolutionManager  SkillEvolutionManager               │
│  RuleChainManager  TeamCoordinator                           │
│  Factory: 实例化  │  Manager: 无状态 + 协调 + EventBus       │
└──────────────────────────┬───────────────────────────────────┘
                           │  Runtime Object
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                    RUNTIME OBJECTS                            │
│  Player ─── Entity + CombatStats + Inventory + SkillManager  │
│  │           + ComboState + AttackEvolutionState              │
│  Monster ── Entity + CombatStats + MonsterAI + Buffs         │
│  │           + MonsterType + TeamRole + Skills               │
│  Boss ───── Monster + BossAI + Arena + Dialogue + Replay     │
│  Item ───── EquipmentItem / ConsumableItem / CharmItem       │
│  Skill ──── SlashSkill / FireballSkill / SelfHealSkill / ... │
│  WorldState: Flags + Counters (rule_* / quest_* / ...)      │
└──────────────────────────┬───────────────────────────────────┘
                           │  tick / update / EventBus
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                    GAMEPLAY SYSTEMS                           │
│  CombatSystem  CombatCoordinator  FloorManager               │
│  ArenaManager  BossSystemDirector (10 subsystems)            │
│  GameplaySystemDirector (Flow/Quest/Story/Ending/Meta)      │
│  MetaSystem  EndingDirector  QuestManager                    │
│  VFXServer  InteractionHandler  GameRenderer                 │
└──────────────────────────┬───────────────────────────────────┘
                           │  EventBus
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                    PRESENTATION LAYER                         │
│  PresentationSystemDirector                                  │
│  Toast / HUD / DamageFloat / CameraShake / FrameFreeze       │
│  BossIntro / RoomMessage / FloorIntro / VFX                  │
└──────────────────────────┬───────────────────────────────────┘
                           │  Render
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                    SCENE LAYER                                │
│  TitleScene  GameScene  FloorSelectScene  TutorialScene      │
│  DeathScene  VictoryScene  CreditsScene                      │
└──────────────────────────────────────────────────────────────┘

                    ┌──────────────────┐
                    │  CROSS-CUTTING   │
                    │  EventBus        │
                    │  ServiceLocator  │
                    │  SaveManager     │
                    │  AudioServer     │
                    │  ResourceManager │
                    └──────────────────┘
```

---

## 二、数据流 (Data → Runtime → Save)

```
┌──────────────┐     load_*_defs()     ┌───────────────┐
│  JSON File   │ ───────────────────→  │  Registry     │
│  (immutable) │                       │  (read-only)  │
└──────────────┘                       └───────┬───────┘
                                               │  get_*_def()
                                               ▼
┌──────────────┐     Factory.create()  ┌───────────────┐
│  Def Struct  │ ───────────────────→  │  Runtime Obj  │
│  (数据/参数)  │                       │  (状态/行为)   │
└──────────────┘                       └───────┬───────┘
                                               │  tick / update
                                               ▼
┌──────────────┐     SaveManager       ┌───────────────┐
│  Runtime      │ ───────────────────→  │  save.json    │
│  State        │                       │  meta_save    │
└──────────────┘                       └───────────────┘
```

### 关键原则

```
Def (JSON, immutable)     ≠    Runtime (C++, mutable)
├── 无 Raylib 依赖              ├── 可引用 Color/Rect
├── 无函数指针                   ├── virtual execute()
├── Registry 只读               ├── Manager 持有
└── 文字/数值/配置               └── 状态/行为/计时器
```

---

## 三、Gameplay 数据流 (EventBus 驱动)

```
  ┌─────────┐    ┌──────────┐    ┌──────────┐    ┌──────────────┐
  │  INPUT  │ →  │ GAMEPLAY │ →  │ MANAGER  │ →  │   EVENTBUS   │
  │ WASD    │    │ Player   │    │ AttackEvo│    │ ATTACK_EVOLVED│
  │ Space   │    │ Monster  │    │ SkillEvo │    │ SKILL_EVOLVED │
  │ 1-4     │    │ Boss     │    │ RuleChain│    │ BOSS_RULE_ACT │
  │ E/I/R   │    │ Item     │    │ QuestMgr │    │ QUEST_COMPLETE│
  └─────────┘    └──────────┘    └──────────┘    │ ENDING_REACHED│
                                                  └──────┬───────┘
                                                         │
         ┌───────────────────────────────────────────────┘
         ▼
  ┌──────────────┐     ┌──────────┐     ┌──────────────┐
  │ PRESENTATION │ →   │   HUD    │     │  Save v3     │
  │ Toast/VFX    │     │ 面板/提示  │     │  qst: / end: │
  │ Shake/Freeze │     │          │     │  atl: / rul: │
  └──────────────┘     └──────────┘     └──────────────┘
```

### 边界规则

| 流向 | 允许 | 禁止 |
|------|------|------|
| Gameplay → EventBus | ✅ emit 事件 | ❌ 引用 UI 对象 |
| EventBus → Presentation | ✅ 订阅 + 读状态 | ❌ 修改 Gameplay 状态 |
| Manager → Manager | ✅ EventBus 中转 | ❌ 直接调用 |
| Gameplay → Renderer | ❌ | ❌ Gameplay 不应知道 Renderer |
| Renderer → Gameplay | ❌ | ❌ Renderer 只读公开字段 |

---

## 四、EventBus 事件类型 (30 个)

```
PLAYER        ATTACK / DAMAGED / DEAD / LEVEL_UP
MONSTER       SPAWN / DIED
BOSS          SPAWN / PHASE2 / LAST_STAND / DEAD
ITEM          PICKUP / USE
RELIC         GAIN
ATTACK        EVOLVED          (G1)
SKILL         EVOLVED          (G1)
BOSS_RULE     ACTIVATED        (G1)
BUFF          APPLIED / EXPIRED
ROOM          ENTER / TRIGGER
NPC           DIALOGUE_START / END
QUEST         ACCEPT / COMPLETE (G2.4)
FLOOR         ENTER / CLEAR
GAME          CLEAR / OVER
ENDING        REACHED          (G2.5)
META          GAIN
```

---

## 五、Save 数据流

```
┌──────────────────────────────────────────────────────┐
│                   save.json (v3)                     │
│  v: floor: maxf: lv: xp: xpt: mhp: chp: atk: pd: md:│
│  act:slash,2,1,35;   pas:iron_skin,1,0,0;           │
│  inv:...  eqw:...  eqa:...  buf:...                  │
│  seed: spr: spd:                                     │
│  atl:2    (AttackEvolution)                          │
│  rul:rule_shadow_charge=1;... (RuleChain)            │
│  qst:0=3;2=4;... (Quest states)                      │
│  end:0;2;4        (Unlocked endings)                 │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│                 meta_save.json (独立)                 │
│  runs: souls: know: memory: nodes:[0,2,0,1,...]     │
│  (跨 Run 持久 — 不在 save.json 中)                    │
└──────────────────────────────────────────────────────┘
```

### 兼容策略

| 场景 | 行为 |
|------|------|
| 缺字段 | `getV("new_field", default)` → 默认值 |
| 旧 act 格式 `Slash,1;` (1 comma) | 兼容解析 → evo=0, use=0 |
| 旧 Skill 名 "Slash" | `_map_old_name()` → "slash" |
| 无 `qst:` | 全部 LOCKED → `update()` 自动解锁 |

---

## 六、Director 依赖图

```
GameScene
 ├── BossSystemDirector ───── 10 boss subsystems
 │    ├── narrative        (对话查询)
 │    ├── evolution        (WorldState → 技能变体)
 │    ├── behavior         (决策权重)
 │    ├── arena            (DangerZone 生命周期)
 │    ├── encounter        (阶段控制)
 │    ├── cinematic        (intro/phase2/death)
 │    ├── timeline         (战斗时间线)
 │    ├── replay           (战斗记忆)
 │    ├── skill_queue      (技能队列)
 │    └── command          (决策→命令)
 │
 ├── GameplaySystemDirector ── WorldState + Narrative + Meta
 │    ├── world_state      (Flags + Counters)
 │    ├── story            (剧情阶段)
 │    ├── quest_mgr        (12 quests, auto-unlock/complete)
 │    ├── rels             (NPC 关系)
 │    ├── ending_dir       (5 endings, C++ 优先级链)
 │    └── run_stats        (本次 run 统计)
 │
 ├── PresentationSystemDirector ── 视觉表现
 │    ├── damage_floats    (伤害数字)
 │    ├── shake / freeze   (震屏/冻结)
 │    ├── room_msg         (屏幕消息)
 │    ├── boss_intro_text  (Boss intro)
 │    └── floor_intro      (楼层/章节入场)
 │
 └── GameFlowDirector ── 12-state 生命周期
      TITLE → NEW_GAME → ENTER_FLOOR → PLAYING
      → BOSS_INTRO → BOSS_FIGHT → FLOOR_CLEAR
      → BOSS_DEAD → PLAYER_DEAD → GAME_CLEAR
      → ENDING → CREDITS → RETURN_TITLE
```

---

## 七、项目规模

| 目录 | 文件数 | 职责 |
|------|:------:|------|
| `src/core/` | 12 | 引擎: Object/Node/SceneTree/InputMap/Logger |
| `src/data/` | 20 | 数据: 10 Def structs + 10 loaders |
| `src/game/entities/` | 18 | 运行时: Player/Monster/Boss/Item/Skill |
| `src/game/systems/` | 21 | 系统: Combat/Floor/Renderer/VFX/Team |
| `src/game/world/` | 28 | 世界: Map/Dungeon/Event/NPC/Quest/WorldState |
| `src/game/boss/` | 18 | Boss: AI/Behavior/Evolution/Narrative/Cinematic |
| `src/game/director/` | 8 | 编排: 4 Directors |
| `src/game/scenes/` | 14 | 场景: 7 Scenes |
| `src/game/scene/` | 6 | GameScene 拆分: Input/Combat/Interaction |
| `src/game/save/` | 2 | 持久化 |
| `src/game/core/` | 4 | EventBus/ServiceLocator |
| `src/game/types/` | 5 | 共享类型 |
| `src/game/audio/` | 6 | 音频合成 |
| `resources/` | 10 | JSON 配置 |
| **总计** | **~172** | |

---

## 八、Development Bible (G1→G3 沉淀规范)

### 1. Runtime/Def 分离

```
Def (JSON) = 不可变配置    Runtime (C++) = 可变状态
```

### 2. Registry Pattern

```cpp
bool load_xxx_defs(const std::string& path);
const XxxDef* get_xxx_def(const K& key);
const unordered_map<K, XxxDef>& get_all_xxx_defs();
bool is_xxx_defs_loaded();
```

### 3. Manager 无状态

```cpp
class XxxManager {
public:
    static void register_listener();
    static void check_unlock(Player*);  // 只读状态, 发送 EventBus
};
```

### 4. EventBus 解耦

```
Gameplay → EventBus → Presentation
         (永不直接调用)
```

### 5. Save v3 追加原则

```
只追加字段, 不修改已有语义
新字段缺失 → getV("new", default) → 默认值
```

### 6. 修改原则

```
新增 > 修改 > 删除 > 重写
每步 ≤5-8 修改文件
```

---

## 九、禁止重构模块 (Architecture Freeze)

| 模块 | 状态 |
|------|:--:|
| Object / Node / SceneTree | 🔒 |
| InputMap | 🔒 |
| EventBus / ServiceLocator | 🔒 |
| CombatSystem 伤害公式 | 🔒 |
| BossAI 状态机 | 🔒 |
| DungeonGenerator (BSP) | 🔒 |
| GameFlowDirector 状态机 | 🔒 |
| SaveManager 核心格式 | 🔒 |
| Player / Monster 生命周期 | 🔒 |
| 6 个 Skill execute() | 🔒 |

---

## 十、依赖方向 (单向, 无循环)

```
JSON ──→ Registry ──→ Factory ──→ Runtime ──→ Gameplay ──→ EventBus ──→ Presentation
  ↑                                                                              │
  └──────────────────── SaveManager (读取 Runtime 状态) ←────────────────────────┘
```

**所有依赖都是单向的。没有循环依赖。**

---

## 十一、G4/G5 路线

```
G1 ✅  Architecture Foundation     (7 steps)
G2 ✅  Content Pipeline             (5 sub-stages)
G3 ✅  Data Driven Framework        (5 sub-stages)
────────────────────────────────────────
G4 🔮  Platform & Mod Support
      · Mod Loader (mod.json + priority merge)
      · Resource Pack (visual_id → file mapping)
      · IModPlugin interface
      · Registry Validator
      · Replay Regression
      详见: docs/G4_PLATFORM_BIBLE.md
G5 🔮  Release Candidate v1.0.0
      · 10000-run automated simulation
      · Balance data collection
      · Performance profiling
      · Plugin implementation
      · Package & deploy
```

## 十二、相关文档索引

| 文档 | 路径 | 说明 |
|------|------|------|
| Architecture Diagram | docs/ARCHITECTURE.md | 本文档 — 架构总览 |
| G4 Platform Bible | docs/G4_PLATFORM_BIBLE.md | G4 平台化规范 |
| World Lore Bible | docs/WORLD_LORE.md | 世界观叙事 |
| Gameplay Loop Design | docs/D1_GAMEPLAY_LOOP_DESIGN.md | 核心循环设计 |
| Changelog | CHANGELOG.md | v0.8.0 版本标签 |
| README | README.md | 项目说明 + 开发进度 |

---

> 本文档为项目唯一权威架构参考。
> 所有新增系统必须遵守此文档中的规范。
> 任何架构变更必须更新此文档。
