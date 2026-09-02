# M2 · SimAI 仪表盘 — Instrumentation Design Plan（只读审计产出, 未写代码）

> 日期: 2026-09-02 · 前置: P1 审计 (V1_4_MILESTONE_PLANNING_REVIEW §P1) + 本次插桩点精读
> 红线: **只加测量, 不动 AI/平衡/胜率**

---

## 0. 审计结论(插桩点全部实地确认)

| 需要的信号 | 现成钩子(实证位置) | 采集成本 |
|---|---|---|
| 死亡时最后受伤来源 | `CombatStats::logged_hp`(防重报) + 全部 7 个 `[DMG]` 打点都已打 attacker 名 (monster.cpp:168 / boss.cpp:151,255,336 / combat_system.cpp `_tick_impl` DOT / game_scene.cpp:908 尖刺,932 岩浆,1647 桶爆) | **极低** — 只缺"把字符串存进 RunResult" |
| 玩家毒 DOT 死亡 | `_tick_impl` (combat_system.cpp:312) 已带 buff id + "Player" 判定 | 极低 |
| 卡墙/恢复 | `[PLAYER-FIX]` 传送 (sim_ai.cpp:215) + 2s 旋转脱困 + loot 看门狗强制下楼 (563-565) | **极低** — 3 个计数器 |
| 战斗帧比例 | `flow.mark_combat()` 已存在但无人消费 (P1 审计) | 低 |
| 搜刮 | 金币在 player->gold; 拾取在 InteractionHandler; 特殊房 triggered 已在存档序列化 (spr) | 低 |
| 武器 | `player->weapon.current_weapon_id()` 存活到 `_collect_sim_stats` | 极低 |
| 超时分类 | 900s 游戏时 (game_scene.cpp:993, 确定性) vs 600s 墙钟 (187/527, **GetTime 非确定**) | 中 (E 项) |
| 构筑 | `calculate_build()` 已在 on_player_dead/clear 调用; 95% "无构筑"是 **profile 不随局内重解析** (P1: sim_ai.cpp:77-80 只在 start() 一次) | 低(记录) / 中(根因) |

**关键判断: A/B/C/D 四组指标 90% 可在 `_collect_sim_stats` 单点聚合完成, 不需要散装打点** — 因为 GameScene 持有 player/monsters/map 全部状态, 而 [DMG] 源字符串只差一个"最后写入者"字段。

---

## 1. M2-A · Run 结果分类 (RunOutcome)

```cpp
// sim_runner.h — RunResult 增补 (全默认值, 旧字段不动)
enum class RunOutcome : uint8_t {
    VICTORY = 0,      // F15 通关
    DEATH_MONSTER,    // 普通怪/精英击杀
    DEATH_BOSS,       // 任一 Boss 战内死亡 (floor 5/10/15)
    DEATH_DOT,        // 毒/持续伤害 (buff tick)
    DEATH_ENVIRONMENT,// 尖刺/岩浆/桶爆
    TIMEOUT_GAME,     // 900s 游戏时上限 (确定性)
    TIMEOUT_WALL,     // 600s 墙钟 (诊断用, 见 E)
    STUCK_RECOVERED,  // 看门狗强制结算 (含 loot 强制下楼)
};
RunOutcome outcome = RunOutcome::VICTORY;   // victory bool 保留 (兼容旧 JSON 读者)
```

**判定逻辑**(全部在 `_collect_sim_stats` 单点, 输入已存在):
- 通关 → VICTORY
- 死亡 → 按死因分类(见 B)
- 到达 993 行超时 → TIMEOUT_GAME (需在分支加 1 行标记, 传给 _collect)
- 187/527 墙钟 → TIMEOUT_WALL
- 战术已闭环: `victory` 字段照旧写 JSON (`"victory":true/false`), **新增 `"outcome":"DEATH_DOT"` 字符串** — 旧报告读者零破坏

## 2. M2-B · 死亡原因 (death_cause)

```cpp
// CombatStats 增补 (combat_stats.h — 1 行)
const char* last_damage_source = nullptr;   // 静态字符串字面量安全, 无人 free
```
- 7 个 `[DMG]` 打点处各加 `combat->last_damage_source = "mon_slime"` 一行(用已有的 name/b.id 字面量)
- `_tick_impl` DOT 分支: `combat->last_damage_source = b.id.c_str()` 改为侧表存 string(规避悬垂) → **设计决定: CombatStats 存 `std::string last_damage_source`**, 7 处赋值, 值语义无悬垂风险, 成本可忽略
- `_collect_sim_stats` 死亡分支: `rr.death_cause = player->combat.last_damage_source` + `death_floor` 已有
- 分类到 RunOutcome: 源含 "boss"/floor∈{5,10,15} 战斗中 → BOSS; 含 "poison"/"DOT" → DOT; 尖刺/岩浆/桶 → ENVIRONMENT; 其余 MONSTER
- **Boss 死亡的精确判定**: monsters 里 is_boss 存活 && 当前楼层 = boss 层 → 直接归 BOSS (比字符串嗅探稳)

## 3. M2-C · 行为指标

```cpp
// RunResult 增补
int combat_frames = 0;        // 有活敌在感知范围/战斗中 mark_combat 窗口的帧数
int rooms_discovered = 0;     // game_map->special_rooms 中 discovered=true 数
int items_picked = 0;         // InteractionHandler 拾取计数 (散装+1 或 ground_items 差分)
int gold_earned = 0;         // player->gold (死亡时快照)
int stuck_teleports = 0;     // [PLAYER-FIX] 传送次数
int stuck_rotations = 0;      // 2s 旋转脱困次数
int loot_watchdog_descends = 0; // 搜刮看门狗强制下楼次数
float combat_participation = 0; // combat_frames / turns (派生)
```
- **卡墙 3 计数器**: DecisionAgent 是 static 内部状态 — 最干净做法: sim_ai.cpp 3 个文件级 atomic/int 计数器 + `_collect` 时读取清零 (单线程 sim, 无竞争; 命名 `g_simstat_*` 前缀明确诊断用途)
- combat_frames: `FlowDirector::mark_combat()` 调用点已覆盖"进入战斗"语义 — GameScene 每帧检测 `flow.in_combat()`(若有) 或 monsters 有活敌 && 距离 < 感知 → 累计; **审计确认 flow_director 有无 in_combat 读取器, 无则加 1 行只读访问器**(不改行为)
- rooms_discovered: `_collect` 时遍历 special_rooms 统计 discovered (数据已在)
- items_picked/gold: `_collect` 快照 (gold 直接读; items 用"开局2药+差分"或拾取点计数 — 采用 **InteractionHandler::pickup 成功路径 +1**, 与玩家同代码路径最真实)

## 4. M2-D · 构筑指标

```cpp
int weapon_type_final = 0;      // player->weapon.current_def()->type (FIST=0..CROSSBOW=5)
const char* weapon_id_final;    // current_weapon_id()
int element_type = 0;           // (int)player->element.type (sim 恒 FIRE — 如实记录, 暴露 sim 覆盖缺口本身就是仪表盘价值)
int level = 0;                  // player->level
int relics_held = 0; int buffs_held = 0;   // 死亡快照
```
- 全部 `_collect` 单点快照, 零打点
- **"无构筑"根因不在 M2 范围** (P1 已定位: profile 只在 start() 解析一次) — M2 只如实记录, 修法留给 M4 之后的 AI 批次

## 5. M2-E · 确定性时间 (必须做, 否则 500 局白跑)

| 泄漏点 | 位置 | 修法 |
|---|---|---|
| 600s 墙钟超时 | game_scene.cpp:187,527 `GetTime()` | **改为累计帧数**: `_wall_frames++` 每帧, 上限 36000 帧 (=600s×60); 完全确定性; 超时归 TIMEOUT_WALL。**删掉 GetTime() 依赖** |
| sim 墙钟起点 | game_scene.cpp:187 `_sim_wall_start = GetTime()` | 同上替换为帧计数器 |
| 死代码 CombatCoordinator::player_attack 用 GetTime | combat_coordinator.cpp:18-19 (无调用者) | **顺手删除死路径** (P1 已确认 0 调用) |
| turns 定义 | `game_time*60` (game_scene.cpp:1556) | 保持 — 本就是确定性帧数, 文档注明单位 |

**验证协议**: 改完跑 `--sim 20 --sim-seed 7` 两遍 → 报告 runs[] 字节级一致 (复用 P0-M2 的 MD5 对比法) + 新字段在两份报告中一致。

---

## 6. BalanceReport JSON 增量 (向后兼容)

```jsonc
// runs[] 每条新增:
{ "victory": false,               // 保留 (旧读者)
  "outcome": "DEATH_DOT",         // M2-A
  "cause": "pool_poison",         // M2-B (死亡源字符串)
  "weapon": "dagger_common", "element": 1, "level": 3,   // M2-D
  "combat_pct": 0.34,             // M2-C 派生
  "rooms": 2, "picks": 3, "gold": 45,                     // M2-C
  "stuck_tp": 0, "stuck_rot": 1, "loot_wd": 0 }            // M2-C
// summary 新增:
"outcome_dist": {"DEATH_MONSTER": 41, "DEATH_DOT": 22, ...}   // 结果分类直方图
"cause_top10": [ {"cause":"mon_slime","n":18}, ... ]          // 死因 Top10
"weapon_perf": { "dagger_common": {"n":30,"win":3,"avg_floor":2.1}, ... }
```
- `enemies[]` 修复(P0): `_collect_sim_stats` 遍历本局遇敌(musters spawn 时 name→id 映射或直接记 name)填充 `enemies_fought`(RunResult 已有字段!) + EnemyStats kills/deaths — **字段本来就有, 只是填充, 零格式改动**

## 7. 工作量与顺序 (预计 2 个提交)

1. **M2-E + M2-A + M2-B** (先确定性+分类: 所有后续数据可信的前提) — combat_stats 1 字段 + 7 打点 + sim_runner 枚举/JSON + 帧数超时
2. **M2-C + M2-D + enemies 修复** — sim_ai 3 计数器 + _collect 聚合 + JSON
3. 每步: `--sim 20` 双跑 MD5 一致性 + 60 测试 + 一次 `--sim 100 --sim-seed 7` 出第一份带完整仪表的基线报告

## 8. 明确不做 (M2 红线重申)

- ❌ 不改 AI 决策/不动 sim_ai 打分 — 只读它已有的恢复动作计数
- ❌ 不修"无构筑"根因 (记录暴露, 修法归 M4 后 AI 批)
- ❌ 不动 balance 数值 / 武器 / Boss
- ❌ 不改 RunResult 旧字段语义 (victory/turns 等全保留)
