# G10.4-B — SWORD Combat Feedback Plan (Design Gate)

> 状态: **待 Review — 未批准前不写代码**
> 日期: 2026-08-31
> 前置: 渲染链只读审计完成（本文档 §1）
> 原则: 修"命中瞬间的时间节奏"，不是堆粒子数量

---

## 1. 审计核心发现

**SWORD 反馈弱的本质：G9 武器化重构时，旧拳套路径的命中反馈三件套（HitStop/命中音/击退）没有迁移到 `_weapon_attack` 路径。**

| # | 量化根因 | 证据 |
|---|---------|------|
| 1 | **武器路径命中 0 次 HitStop** | `_weapon_attack`（player_controller.cpp:584-738）全函数无 `trigger_freeze`；旧路径有重击 0.075s/暴击 0.03s/击杀 0.14s（:752-770），装备 SWORD 后全部失效 |
| 2 | **命中音=空挥音** | 武器路径只播 swing 音（weapon_executor.cpp:242-245）；`play_sfx("hit")` 只在未走的旧路径（combat_coordinator.cpp:54） |
| 3 | **暴击数字 alpha 溢出 bug** | game_scene.cpp:2036 除数硬编码 `0.6f`，暴击寿命 0.85s → alpha=361 溢出为 105——**暴击数字前 0.25s 只有 41% 亮度甚至隐形**（恰是玩家注意力最高窗口）；且 `DMG_FLOAT_SCALE_CRIT=1.6` 常量从未接线，暴击无字号放大 |
| 4 | 爆点积分亮度低 | 32×32 贴图仅 8 条 1px 放射线+6×6 核（可见直径 ~20px），hit_flash 0.15s 纯线性淡出无首帧保持 |
| 5 | stage2 无终结感 | 只有半径变大+shake14；无更长 freeze/无 KILL_SLOWMO，flash 反而最短（0.12s）；旧路径重击还有 24px 击退，SWORD 没有 |
| 6 | 震幅弱+JSON 死数据 | stage0 峰值 ±3px/120ms 基本无感；weapons.json 的 camera_shake/windup/hit_frame/cancel_window 四组字段全部零消费 |

次要：spark 静止无轨迹（生成点即终点）、米黄配色低饱和（依赖黑底弧衬托）。

**遮挡排除**：VFX 绘制在怪物之上（game_scene.cpp:1929-2032 顺序），渲染顺序不是问题。
**时机排除**：按键→判定→VFX 同帧（0 延迟），不吞反馈。

---

## 2. 修正方案（最小批量，2 文件 ~45 行）

### Fix 1: 命中 HitStop + 命中音（player_controller.cpp results 循环，~15 行）

在 `_weapon_attack` 的 results 结算处（:734-737）按 stage 分级接入**已有的** CombatFeelSystem：

| 命中情形 | freeze | 依据常量 |
|---------|--------|---------|
| stage 0/1 普通命中 | 0.03s | `LIGHT_HIT` |
| stage 2 重击命中 | 0.075s | `HEAVY_HIT` |
| 任意段暴击 | 0.07s | `CRITICAL_HIT` |
| 任意段击杀 | 0.14s | `KILL_SLOWMO` |

同处补 `play_sfx("hit")`（合成音已有，零新资产）。

> freeze 是既有系统（旧路径/连击里程碑/受击都在用），确定性不受影响（纯表现层计时）。

### Fix 2: 暴击数字修复（game_scene.cpp，~5 行）

- 除数 `0.6f` → 按 damage number 自身寿命计算（存 max_lifetime 或用现有字段），根除溢出
- 暴击字号 ×1.6（接线闲置常量 `DMG_FLOAT_SCALE_CRIT`）

### Fix 3: stage2 终结感（player_controller.cpp SWORD case，~4 行）

- flash 时长 0.12s → 0.18s
- 命中点 explosion 火花 8 → 12 粒
- （freeze 0.075s + 击杀 KILL_SLOWMO 已由 Fix 1 覆盖 stage2 的"重"）

### 不做（本批明确排除）

- ❌ **击退**：位移影响 gameplay/平衡/确定性（旧路径有，但接回需要走 WeaponExecutor 结算层，超出视觉批次范围）→ 留待单独决策
- ❌ 不重写粒子系统（spark 加速度向量）/ 不换爆点贴图密度 / 不改米黄配色（配色与 A.1 校色后的新地板对比度需实机再评）
- ❌ 不接 weapons.json 死数据（camera_shake/windup 接线是数据架构活，单开批次）
- ❌ 不新增 VFX kind、不加光照/shader

### 文件影响范围

| 文件 | 改动 | 行数 |
|------|------|------|
| `src/game/player_controller.cpp` | results 循环 freeze/sfx + SWORD stage2 微调 | ~20 |
| `src/game/scenes/game_scene.cpp` | 暴击数字除数 + 字号缩放 | ~6 |
| 测试 | 无新测试（表现层；freeze 逻辑走既有 Presentation 路径） | 0 |

---

## 3. Review Gate

- [ ] **D-A**: Fix 1 命中 HitStop 分级表 + 命中音 — 同意？
- [ ] **D-B**: Fix 2 暴击数字 bug 修复 + 字号 1.6× — 同意？
- [ ] **D-C**: Fix 3 stage2 终结感微调 — 同意？
- [ ] **D-D**: 击退本批不做（留单独决策）— 同意？

批准后：Fix 1 → Build/CTest → Fix 2+3 → Build/CTest → 桌面同步 → 实机 Review（重点：连续攻击怪群，感受"砍中停顿"与三段递进；暴击数字是否醒目）。
