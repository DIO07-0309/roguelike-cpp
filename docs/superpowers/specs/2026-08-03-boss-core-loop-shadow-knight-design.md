# BOSS 核心环革新设计 — 暗影骑士样板

日期：2026-08-03
状态：已获用户批准（蓝图 + 机制规格）

## 1. 目标与范围

在既有 BOSS 系统扩展点上激活"核心环"：连招骨架 + 阶段驱动 + 自适应反制 + 演出点缀。
样板为 F5 暗影骑士（bosses.json: `shadow_knight`），验收通过后推广到其余 5 个 Boss。

体验目标：动作挑战为主（读招拆招）、策略为辅（性格克制）、演出点缀。难度：中等偏难（新手死 2-3 次学会拆招）。

## 2. 暗影骑士完整机制规格（用户需求）

### 2.1 主动技能

| 技能 | 效果 | 数据要点 |
| :--- | :--- | :--- |
| 远程弹幕 | 扇形 3-5 发弹幕，命中附带减速 | 复用 D2 `ProjectileFactory::enemy_projectile`；减速 = `slow` buff（buffs.json 已有，3s/3 层） |
| 近战扇形斩 | 扇形范围攻击，命中后中毒 2S | 复用 `get_targets_in_cone`（skill.cpp 已有）；中毒 = 新增 `poison2s` buff（buffs.json，duration 2.0，tick 同 poison 3 伤/0.5s） |
| 瞬移 | 闪现至玩家侧翼 + 接扇形斩 | CD 12S 独立计时；VFX 闪烁 + 残影 |

### 2.2 被动机制

| 机制 | 规则 |
| :--- | :--- |
| 暗影之盾 | 12S 周期：盾持续 7S（免疫玩家 80% 伤害）+ 无盾窗口 5S；盾出现播报 + 盾环 VFX |
| 受击召唤 | 受到伤害触发：1 远程（archer）+ 2 近战（orc）；冷却 = 上一波小怪全部死亡后 3S 内不可再召唤 |
| 移动速度 | 中等（保持现状） |

### 2.3 连招模板（SkillQueue 驱动）

- 试探连招（OPENING，HP>75%）：`弹幕 → 扇形斩 → 普攻×2`
- 狂暴连招（PRESSURE，HP≤75%）：`瞬移+扇形斩 → 弹幕 → 召唤`
- 压制连招（CONTROL，HP<40%）：狂暴连招变体（弹幕数量 +2，扇形角度 +30°）
- 连招间留 0.4-0.8s 读招窗口；连招尾带 0.8s 收招硬直（玩家输出窗口）
- LAST_STAND（HP<15%）沿用现有：全技能 CD×0.5

## 3. 核心环组件设计

### 3.1 连招系统（M4a）
- 激活 `BossSkillQueue`（boss_types.h 已有：enqueue/start/advance）：`boss_decision_to_command` 产出当前连招 → queue 顺序执行 → BossAI 状态机按 queue 当前项进入对应 BossState
- `BossDef` 新增 `combos` 配置（JSON）：`{ "id": "probe", "commands": ["RANGED","CONE_ATTACK","NORMAL","NORMAL"], "interval": 0.6, "end_delay": 0.8 }`（数据驱动，遵守 snake_case）
- 新 BossState：`RANGED_BARRAGE`（弹幕）、`CONE_ATTACK`（扇形斩）、`BLINK`（瞬移）；新增对应 BossSkill 子类（照 WhirlwindSkill 模式）
- `_next_cycle_skill` 硬编码循环退役，仅保留普攻穿插

### 3.2 阶段驱动 + 战场（M4b）
- `BossEncounterController::phase()`（已有，零调用）接入：phase 驱动连招模板切换（Open/Probe、Pressure/Rage、Control/Press）
- arena：PRESSURE 起生成暗影墙（shadow_wall，配置已存在）；CONTROL 阶段 `ArenaEventType::INTENSIFY`（生成间隔减半，execute_event 已有分支）
- 阶段切换播报（room_msg + 变体名 variant_name 激活："暗影风暴！"）

### 3.3 自适应反制（M4c）
- 行为层已具备（boss_behavior.cpp:66 权重表已写 mem.dodges>8 等），只补数据埋点：
  - 闪避：技能蓄力结束时玩家不在预警圈内 → `record_dodge()`（利用 D2 预警判定，无新翻滚）
  - 喝药/回血：Boss 战中 `heal_player` 调用 → `record_heal()`
  - 连击：`player.combo.count >= 4` → `record_combo()`
- 权重微调：dodge 多 → 狂暴连招频率↑；heal 多 → 连招间隙↓；combo 高 → 召唤优先
- 战报数据修正：`dmg_done/melee_hits` 只统计对 Boss 命中（当前被全层小怪污染）

### 3.4 演出点缀（M4d）
- Phase2 转场接 `emit_boss_phase2_vfx`（已存在零调用）
- 盾/瞬移/连招名播报；瞬移残影 VFX

## 4. 数值与难度

- 基础数值沿用（HP 1125/ATK 57 @F5 曲线），强度靠机制而非数值
- 盾周期制造输出节奏：无盾 5S 窗口是主要输出期
- 受击召唤限制 3S 冷却防止召唤刷屏；召唤怪血量低（玩家 1-2 刀）
- 弹幕可被走位躲（3-5 发，角度固定）；扇形斩预警圈 0.4s

## 5. 实施顺序与验收

| 里程碑 | 内容 | 验收 |
| :--- | :--- | :--- |
| M4a | 连招系统（queue 激活 + 3 新技能 + JSON combo） | 暗影骑士按模板出招，玩家可读招 |
| M4b | 阶段驱动 + 暗影墙/INTENSIFY | HP 阈值切换连招与战场 |
| M4c | 自适应埋点 + 数据修正 | 玩家行为影响连招选择，战报数据真实 |
| M4d | 演出 + 数值实测调平 | 中等偏难可通关，桌面版同步 |

每里程碑：构建 0 error → validator → 运行冒烟 → 同步桌面打包版 → 提交。

## 6. 推广路径（样板验收后）

其余 5 个 Boss 各自补充：combo 模板（复用 3 新技能或换技能组合）、阶段/战场配置、兑现注释欠账（吸血鬼吸血、魔像三连、亡灵法师尸爆）。
