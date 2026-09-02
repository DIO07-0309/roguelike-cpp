# G10.9-A — Multi-Slot Save System Audit（只读审计报告）

> 审计日期: 2026-09-02 · 审计人: opencode (z-ai) · 状态: **只读完成，未改任何代码**
> 方法: 3 路并行源码审计（SaveManager 字段/迁移 + Meta/Endings/Hints + Menu/Settings/Mirror）+ 人工复核关键行号

---

## 0. Executive Summary

当前是**单槽存档**: `saves/save.json`(局内) + `saves/meta_save.json`(账号) 两个文件,外加 5 个离线 RL 工件。审计发现 **4 个现存 bug**(与多槽无关也该修)和 **2 个归属争议**。多槽化最大的坑不是"复制三个 JSON",而是: ①meta 从不加载导致的覆盖 bug ②结局解锁从不落盘 ③镜像记忆混在局内档 ④音量/全屏无任何持久化。

---

## A. 当前数据流图(真实调用关系)

```
main.cpp 启动
 ├─ SaveManager::save_exists() / load_save()  [main.cpp:296-298]  ← 只用于探测, 结果丢弃
 └─ TitleScene::_enter_tree
     ├─ has_save = SaveManager::save_exists()          [title_scene.cpp:88]
     ├─ max_floor = load_save()->max_unlocked_floor     [title_scene.cpp:90-91] ← 选关数据源!
     └─ 菜单分发 (_activate, title_scene.cpp:17-83)
         ├─ N 新游戏 ──► GameScene::new_game()          [game_scene.cpp:155-227]
         │                └─ 首局: element_select UI → _input 补流程 [1503-1528]
         │                   └─ g_meta.load()            [1517] ← 唯一热路径 load!
         ├─ C 继续   ──► load_save() → load_saved_game()[title_scene.cpp:31-58 → game_scene.cpp:236-276]
         │                ⚠ 此路径【从不】g_meta.load()
         ├─ F 选关   ──► FloorSelectScene (max_unlocked ← save.json maxf)
         │                └─ 有档: load_saved_game(floor, seed=0)  [floor_select_scene.cpp:79-84]
         │                └─ 无档: new_game() + enter_floor(floor) [86-91]
         └─ T 教程 / G 全屏 / Esc 退出

局内写盘点 (saves/save.json, save_manager.cpp:43-215, fopen"w" 全量覆盖):
 ├─ 上下楼自动存档  _activate_stairs   [game_scene.cpp:1798-1804]
 └─ Esc 存档退出    game_scene_input   [game_scene_input.cpp:95-101]

局内写盘点 (saves/meta_save.json, meta_progression.cpp:117-142):
 ├─ mark_hint_shown  每条首遇提示即写  [meta_progression.cpp:210-215]
 ├─ end_run          死亡结算          [meta_progression.cpp:92] ← 被【两处】调用!
 ├─ reward_from_ending 通关结算        [meta_progression.cpp:109]
 └─ game_flow_director.on_game_clear   [game_flow_director.cpp:129-130] ← 与上重复发钱!

死亡: 存档保留, 不删除 (death_scene.cpp:43); delete_save() 存在但【全工程 0 调用】[save_manager.cpp:585]
```

### save.json 完整字段清单 (v:4, 手写 key:value 行格式, 非 JSON 库)

| 行键 | 内容 | 归属域 |
|---|---|---|
| `v` | 格式版本号 4 (**只写不读** — 加载端无版本分支!) | — |
| `floor` / `maxf` | 当前层 / 解锁层 | Run |
| `lv` `xp` `xpt` | 等级/经验 | Player |
| `mhp` `chp` `atk` `pd` `md` | HP/攻防 | Player |
| `gld` `key` | 金币/钥匙 | Player(经济) |
| `act:` `pas:` | 主动/被动技能 id,lv,evo,use 分号表 | Player |
| `inv:` | 背包 name,rarity,slot,atk,pdef,mdef[,wpn_id] | Player |
| `eqw:` `wpn:` `eqa:` | 已装备武器/armor + weapon_def_id | Player |
| `buf:` | Buff id,stacks,remaining,tick_timer | Player |
| `seed` | 地牢种子 (同层重进复现布局) | Run |
| `spr:` `spd:` | 特殊房间 触发/发现 位图 | Run |
| `rlc:` | **仅 RUN scope** 圣物 id,scope | Run ⚠ |
| `atl` | attack_evo_level 普攻进化 | Player |
| `rul:` | rule_counters k=v 表 | Run |
| `qst:` | quest_states id=state 表 | Run |
| `elem:` | 元素 type,lv,xp,initialized | Player |
| `end:` | 已解锁结局 列表 | **争议** ⚠ |
| `mra:` `mrb:` | 镜像 AI 记忆 72+72 float | **争议** ⚠ |
| ❌ 无玩家坐标 / 无游戏时长 / 无房间布局(靠 seed 重建) | | |

### meta_save.json 完整字段 (meta_progression.cpp:117-142)

| 键 | 内容 | 说明 |
|---|---|---|
| `runs` | 总局数 | |
| `soul` `know` `memory` | 三种局外货币 | |
| `nodes` | 10 个天赋等级 | ⚠ 当前文件为 `[]` — 写入时未 load_from_defs |
| `hints` | 已显示首遇提示 id 数组 | **G10.8 首遇教程所在, 已正确归属 Meta** ✓ |

### 其余磁盘文件

| 文件 | 写者 | 级别 |
|---|---|---|
| `rl_qtable.json` + `rl_mirror_q_{4风格}.json` | 仅 `--rl-train/--rl-mirror` CLI 离线训练 [rl_runner.cpp] | 账号级(离线工件), 运行时只读 [boss_system_director.cpp:196-206] |
| `game.log` `crash.log` | logger | 诊断 |
| `reports/balance_report.json` | 仅 `--sim` | 离线工件 |
| ~~`mods/config.json`~~ | save_config **死代码 0 调用** | — |
| ~~行为录制 dump~~ | save_to_file **死代码 0 调用**, g_behavior 内存态, 进程即失 | — |
| ~~replay 录像~~ | save_replay **死代码 0 调用** | — |

---

## B. 数据归属矩阵(按代码实证, 非预设)

| 数据 | 当前文件 | 推荐归属 | 依据 |
|---|---|---|---|
| Player HP/属性/技能/背包/装备/Buff/元素/进化 | save.json | **Slot** | 纯局内成长, 换档应清零 |
| 当前层/种子/特殊房间/rule_counters/quests | save.json | **Slot** | 单局进度 |
| 金币/钥匙 | save.json | **Slot** | 局内经济 |
| RUN 圣物 | save.json `rlc:` | **Slot** | B13 注释: 本来就局内 |
| max_unlocked_floor (选关依据) | save.json `maxf` | **Slot** | 跟档走 (见争议#1) |
| **unlocked_endings** | save.json `end:` | **Meta** | 跨局收集要素, 放 Slot 则删档=丢全收集 ⚠ 现状bug: **从不落盘**(见 bug#2) |
| **mirror 记忆 mra/mrb** | save.json | **Slot** | 设计语义="该玩家对应的镜像进化" — 若三档是不同玩法风格, 混在账号会让 A 档养的镜像打 B 档玩家 |
| rl_mirror_q_*.json | 独立文件 | **Meta/账号** (保持独立文件即可) | 风格分类全局工件 |
| 首遇提示 hints | meta_save.json | **Meta** ✓ 已正确 | 与用户期望一致: 删档不清教程 |
| 局外货币 soul/know/memory, runs | meta_save.json | **Meta** ✓ | |
| 天赋 nodes | meta_save.json | **Meta** ✓ (但**无消费者**, 死树) | |
| **音量/全屏/键位** | **不存在** | **Meta** (新增 settings) | 全硬编码 [scene_tree.cpp:57-59], 每次启动重置 |
| 行为录制 g_behavior | 仅内存 | 不持久化 (维持现状) | 死代码本就未挂 |

### 归属争议(需 Design Gate 拍板)

1. **max_unlocked_floor**: 放 Slot 意味着"删档=选关重置"。若希望选关是账号级成就 → 应镜像一份到 Meta。**建议: 主数据放 Slot, Meta 只保留 `best_floor` 用于展示** (选关跟当前活跃档走, 符合主流 roguelike)。
2. **unlocked_endings**: 已在矩阵标 Meta。迁移时把 save.json `end:` 行整体搬到 meta。**但注意现状它根本不落盘** — 先修 bug#2 再迁移, 否则搬的是空数据。

---

## C. 三存档迁移方案评估

| 维度 | 方案 A: `save_slot_1.json` | 方案 B: `saves/slot_1.json` (用户倾向) |
|---|---|---|
| 现有 `saves/` 目录 | 平铺混入 rl_*.json/meta | 分层干净 |
| 扩展性 (回放/截图/每档设置) | 每个都是前缀爆炸 | 每档一个子目录自由扩展 |
| 现有代码侵入 | `_save_path()` 一处改动 | 同样一处, 但顺带理顺目录语义 |
| RL 工件归属 | 继续混在账号层 | 可顺势 `saves/rl/` 归档 |
| 迁移成本 | 相同 | 相同 |

**结论: 同意方案 B**, 且建议完整布局:

```
saves/
├── slot_1.json        ← 每档单文件(现状格式照搬, 后续 G10.9-B 再议要不要分 meta 摘要)
├── slot_2.json
├── slot_3.json
├── meta_save.json     ← 账号级(货币/hints/endings/settings)
└── rl/                ← 账号级 RL 工件(rl_qtable, rl_mirror_q_*)
```

**每档单文件 vs 每档目录**: 当前提案(单文件)够用 — slot 数据全部在 `save_game` 一次写完, 无分步写需求。若未来要存每档回放/行为快照再升目录, 接口上用 `_save_path(slot_id)` 一层抽象即可平滑演进。

**SlotSummary 依赖的迁移注意点**: 菜单要显示楼层/等级/时长, 但 save.json **没有 play_time** — G10.9-B 需顺带加 `time:` 字段(现在加零成本, 以后补要迁移旧档)。`biome_name` 可由 floor 派生(1-4 微光苔原/5 岩浆/6-9 幽暗沼泽/10 黄沙/11-14 深渊/15 终焉), 无需存。

---

## D. SlotSummary 设计(按审计实证)

```cpp
struct SaveSlotSummary {
    bool exists = false;          // save_exists(slot)
    int  slot_id = 0;             // 1-3
    int  current_floor = 1;       // floor:
    int  max_floor = 1;           // maxf:
    int  player_level = 1;        // lv:
    int  element_type = 0;        // elem: [0] — 展示火/冰/毒图标
    float play_time = 0.0f;       // time: 【字段待 G10.9-B 新增, 现不存在】
    // biome_name — 由 current_floor 派生, 不存不读
};
```

读取接口建议: `SaveManager::read_slot_summary(int slot)` — **轻量只读**(fopen 扫描 5 行), 不构造 Player 对象。现 `load_save()` 会重建全套技能/背包(需要 Registry 已初始化), 摘要页绝不能走它。

---

## E. 现存 Bug 清单(审计副产物 — 迁移前必修)

| # | 严重度 | 问题 | 证据 |
|---|---|---|---|
| 1 | **Critical** | 继续游戏路径**从不 g_meta.load()** → 首遇提示查空表 → mark_hint_shown → **用默认值整文件覆盖 meta_save.json**(货币/runs 清零) | game_scene.cpp:236-276 无 load + meta_progression.cpp:210-215 即写 |
| 2 | High | **结局解锁从不落盘**: EndingDirector::begin() 只在死亡/通关时入 _unlocked, 但两个存档点都在它之前 — `end:` 行只会往返旧值 | ending_director.cpp:43-51 vs game_scene.cpp:1800 / game_scene_input.cpp:97 |
| 3 | High | **end_run 双调用** → runs 双计数、死亡奖励双发 | gameplay_system_director.cpp:53 + game_scene.cpp:1033 |
| 4 | Medium | 通关奖励双发 (reward_from_ending + game_flow_director add_currency), begin() 双跑 | game_scene.cpp:1835-1837, game_flow_director.cpp:83/125-130 |
| 5 | Medium | `v:4` 版本行只写不读 — 加载端零迁移分支, v1/v2/v3 老档靠 getV 默认值兜底"碰巧兼容" | save_manager.cpp:59 vs 233-266 |
| 6 | Low | meta nodes 死树: upgrade_node/node_level/permanent_bonus 全工程 0 调用; HINT_IDS 声明 12 实装 6; delete_save 无 UI | meta_progression.cpp:42-75/199-203 |
| 7 | Info | meta 加载缓冲区 2048 字节硬上限 (hints 增长天花板) | meta_progression.cpp:148 |

**Bug#1 与用户观察吻合**: 现场 meta_save.json = `{"runs":0,"nodes":[],"hints":[hud_intro,element_select]}` 正是该 bug 产生后的形状(runs 被抹回 0)。

---

## F. G10.9-B ~ E 路线建议(锁定项)

```
G10.9-A 只读审计 ✅ (本文档)
   ↓
【Design Gate — 需用户拍板 3 项】
   1. max_floor 归属: Slot主 + Meta.best_floor 展示? (我方建议)
   2. endings 迁 meta_save.json? (我方建议: 是, 顺带修 bug#2)
   3. 镜像记忆随 Slot? (我方建议: 是 — 三档即三种玩法, 镜像分档进化)
   ↓
G10.9-B  Slot Infrastructure
   - _save_path(slot) 抽象 + 旧 save.json → slot_1.json 一次性迁移(启动检测)
   - 修 bug#1 (boot 时统一 g_meta.load) / bug#2 (通关路径补存档点) / bug#3/#4 (双调用去重)
   - save.json 增加 time: 字段 (v:5 — 并首次真正读版本号做 v≤4 兼容)
   ↓
G10.9-C  Main Menu Slot Selection (SlotSummary 只读 + 三卡 UI)
   ↓
G10.9-D  New Game / Continue / Level Select × Slot 语义
   ↓
G10.9-E  Delete + Migration + Regression (60 tests + 新增 slot 迁移测试)
```

**交互流确认**: 继续游戏→选档→继续 / 选关→选档→读该档解锁层 / 新游戏→空档直进, 满档→选牺牲档→二次确认→覆盖 — 与用户提案一致, 无异议。

---

## G. 审计边界声明

- 三路 agent 只读源码 + 人工复核 save_manager/meta/title 三处关键行号, **未运行游戏、未写任何文件**
- rl_*.json / mods/config.json / 行为录制 dump 的"死代码"结论基于全工程 grep 0 调用
- `saves/*.user_backup` 为用户手工备份, 非游戏产物
