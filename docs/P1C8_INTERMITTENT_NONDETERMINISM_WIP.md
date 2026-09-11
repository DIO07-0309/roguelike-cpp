# P1-C8 · 间歇非确定性调查 — 已结案 (RNG-002)

> 状态: **已修复并验证** (2026-09-11)。HEAD=32a1d00 (v1.4.14) + RNG-002 修复。
> 探针已全部删除, 工作区仅剩 vfx_server.cpp 修复 diff。
> 症状: 同 exe 同 seed `--sim 12 --sim-seed 3`，约 12~50% 批次分岔
> (v1.4.13 "24 连跑一致"结论已被推翻, 假阳性同 P1-C3 教训)。

## 1. 复现协议 (可靠触发)

```powershell
# 并行 8 进程 (串行单跑分岔率低且慢; 并行时 2~4 对分岔)
$jobs=@(); foreach($p in 1..4){
  $a=Start-Process .\build\roguelike_cpp.exe -ArgumentList "--sim 12 --sim-seed 3" `
     -RedirectStandardOutput "reports\p1c8\X_$($p)_a.out" -NoNewWindow -PassThru
  $b=Start-Process .\build\roguelike_cpp.exe -ArgumentList "--sim 12 --sim-seed 3" `
     -RedirectStandardOutput "reports\p1c8\X_$($p)_b.out" -NoNewWindow -PassThru
  $jobs+=$a.Id+$b.Id }
Wait-Process -Id $jobs
# MD5 对比; 分岔形态是确定吸引子 (每批同哈希: 无探针版 35E63B432D)
```
注意: 探针密度会改变分岔率 (观察者效应) — fp+pos+spd 探针齐时曾 14 连跑
零分岔。**并行跑是更可靠的触发器**。cmd `>` 重定向 (PowerShell `*>` 会丢输出)。

## 2. 证据链 (从症状到原点, 逐层收网)

1. **表层**: run 边界日志错位; "F3 毒 tick 差 1 条" (CHANGELOG 初判) 是下游症状
2. **哈希层** (FPDIAG, 每 10 帧 compute_state_hash): 首异在 run index 8 (第 9 局)
3. **位置层** (POSDIAG, %.6f): run 8 内 **F1/F2 逐行一致**; F3 段 827 行一致后
   **t=18.017→18.183 分岔**: 史莱姆 id=329 位置双方 6 位小数完全一致
   `(722.745117, 164.084961)` → 10 帧后 **A 原地冻结 / B 移动 +2.67px**
   (x→725.411743)。窗口内**零战斗/buff/拾取日志** (玩家位置双方也一致)
4. **雪崩层**: F4~F9 进层瞬间 (t=0.000) 全分岔 (出生点/怪池/HP/buff 剩余);
   F9 怪池 A=[356,357] B=[362,363] (B 含哥布林召唤师→再放大 id 差)
5. **跨局泄漏**: instance_id 全局计数器 + gameplay rng() 疑似不按局重播 →
   run 9+ 也分岔 (run 9 F1 同位置怪 id A=362 B=366; run 9 独立分岔点
   t=20.017 B 被 slow 而 A 无) — 待确认 rng() 是否 per-run reseed

## 3. 原点画像 (run 8, F3, t=18.017→18.183)

- 一只史莱姆 AI 停走分歧: A 停 (=patrol_dir {0,0}? / 房间边界拦截? / leash 折返?)
  vs B 继续移动 2.67px (=10 帧持续)
- 输入状态 (位置/HP/怪池) 双方逐位一致 → 分歧源在**未探测状态**:
  MonsterAI 内部 (`_patrol_timer`/`_patrol_dir`/`provoked`/`_monster_room`)
  或 **gameplay RNG 流位置** (若 `_pick_new_dir` 的 rng() 掷点不同: 30% 出 {0,0})
- 30% 停走概率 + 无外部事件 → **首选假设: RNG 流在该窗口前已错位**
  (某处消耗了不同次数的 rng() 但未产生可观测差异)

## 4. 已排除

- dt: sim 定步长 1/60 (main.cpp:315) ✓
- `last_attack_wall_time`/`_swing_start`/game_map.draw 的 GetTime: 纯渲染路径 ✓
- AI 文件 (src/ai/**, src/game/ai/**) 无墙钟 ✓
- Logger 同步单线程 (锁+fflush), 不改状态; 但 I/O 量影响时序布局 (观察者效应)
- pre_hp 裸指针快照 (combat_coordinator.cpp:72): 时停路径残留风险, 本例窗口
  无时停, 非本例根因; 列**顺带清理候选**
- env 装置 (毒池/尖刺/桶): `ao.timer += dt` 确定 ✓

## 5. 未覆盖缺口 (下一步从这里开始)

1. ~~ai.cpp GetTime/chrono~~ **已排除** (2026-09-11 全局 grep: ai.cpp/team_coordinator/
   hit_detection/projectile 无墙钟; combat_coordinator.cpp:72 `pre_hp` 无序容器只读快照,
   无迭代序副作用)
2. **rng() 重播种已确认**: 每局 `rng.seed(next_seed())` → draws 回绕 (uint64 下溢) 出现在
   每 run 首帧 spike `delta_draws=18446744073709...` (非 bug, 是重播种清零计数)
3. ~~AI 内部状态探针~~ 已被 RNGSPIKE/RNGSITE 调用点证据替代 (见 §9)

## 6. 结案清理清单 (全部完成 2026-09-11)

- [x] 删除 game_scene.cpp `_tick_replay_hash()` 内全部探针 (含 RNGSPIKE/RNGSITE)
- [x] 删除 combat_system.h `CountingRng` call_sites 环形缓冲
- [x] 重建 Release 0 errors + 60/60 ctest + world_validator 通过
- [x] 冒烟验证: 16 对并行 (32 进程批) **零分岔**, 同哈希 061A2BE6...
- [x] 修复: RNG-002 方案 A (vfx_server.cpp 11 处 rng() → visual_rng())
- [ ] 顺带候选 (讨论后定, 未做): combat_coordinator.cpp:72 pre_hp 裸指针快照
      (本例已证非根因; 仍列清理候选)
- [x] CHANGELOG 记 P1-C8 根因/修复
- [x] README Current Limitations 更新
- [x] 同步桌面打包版 (exe 在根目录) + commit
- [x] 修正 RELEASE_CHECKLIST.md §4 的 P1-C8 披露行状态

## 7. 证据文件 (reports/p1c8/, 勿删)

- `probe_*.out` 60 帧哈希探针批 (1/4 分岔) — 4 连跑定位
- `fine_*.out` 10 帧哈希 (3/6 分岔) — 窗口收窄到 10 帧
- `pos_*.out` 14 连跑零分岔 (观察者效应记录)
- `px_*` / `spd_*` / `hp_*` 并行批 — 分岔吸引子 + spd/hp 探针数据
- 分析脚本在 `C:\Users\HP\AppData\Local\Temp\opencode\p1c8_*.py` (临时目录,
  可能被清): 核心=按 [SIM] n/12 分局 → 按 "进入第N层" 分层 → zip 逐行/逐记录比
  → 首异上下文打印。**zip 按索引对齐在局内时长不同时会错位, 结论只信
  "逐行一致到第 N 行" 与 "同帧同 id 记录对比" 两种** (spd_runs 的 run-9 结论
  与 f8_diff 的 run-8 结论冲突即此因, 以 f8_diff 的原始行对比为准)

## 8. 关键数据备忘

- run 8 各层首怪 id: A F8=349 一致; F9 A=356 vs B=358 ← 首个 id 分岔层
- run 8 F3 原点: t=18.017 一致 → t=18.183 分岔; 玩家 A=(541.33,32)→(548,58.67)
  双方一致; 怪 329 (史莱姆 hp=21) + 330/331 (哥布林弩弓手 hp=31)
- 下游确认: F4 入口 A p=(160,128) B p=(224,192); F5 入口 A blessing:2:1.65
  B 无; F8 入口 A attack_up:6.00 B 1.03 — 均为**时间线错位的下游效应**
- 吸引子哈希: 无探针 35E63B432D; hp 探针版 8CC60C64DF (分岔形态可复现)

## 9. RNGSPIKE/RNGSITE 调用点解析 (2026-09-11, 根因锁定)

### 9.1 方法论 (可复用)

- rs_*/sp_* 批 = 带 RNGSPIKE(delta>40) + RNGSITE(最近 128 次调用返回地址
  环形缓冲) 的探针版; `__builtin_return_address(0)` 因内联记录的是**外层调用点**
- **ASLR slide 唯一确定法**: nm 取 T/t 符号表 → 全量 objdump -d 提取所有
  `call` 返回地址集合 → 64KB 网格扫 slide, 要求 9 个观测地址**全部**精确落在
  call 指令边界 → 唯一解 `slide=0x7ff5ad100000` (松散过滤有 36 个假候选,
  语义合理性不构成证据, **call 边界才是硬验证**)
- 环形缓冲输出顺序: 文件序 = 时间倒序 (cursor 指向下一个写入槽=最旧,
  dump 从 cursor 开始 → 输出末尾才是最新)

### 9.2 分岔帧调用点 (rs_2 批, run9, 原点 tick delta=117, B 独有)

| 静态地址 | 符号 | draws |
|---|---|---|
| 0x140109eed | handle_input+0x239d → `VFXServer::spark_burst` | 24 |
| 0x14000d0b10 | MonsterAI::update+0x550 (巡逻掷骰区) | 3 |
| 0x14014119e | _process+0x14de → `VFXServer::explosion` | 48 |
| 0x140141343 | _process+0x1683 → `VFXServer::spark_burst` | 18 |
| 0x1401412dc | _process+0x161c → `VFXServer::explosion` | 24 |
| 0x140122067 | on_monster_killed+0x1667 → `generate_random_item` | 4 |
| 0x140105349 | _kill_target+0x19 → `GameScene::_on_monster_killed` | 1 |
| 0x140106622 | _weapon_attack+0x3f2 → `VFXServer::spark_burst` | 5 |

- `_process` 的三个调用点 = game_scene.cpp:857-929 **元素事件 VFX switch**
  (FIRE_HIT/CRITICAL/ICE/POISON case 内联 explosion/spark_burst)
- B 同帧多耗 90 draws 但帧末状态哈希**仍一致** → 纯 VFX 掷骰, 不产生
  gameplay 可观测差异 (WIP §3 "RNG 流错位但无外部事件"假设成立)

### 9.3 根因 (RNG-002): vfx_server.cpp 视觉掷骰吃 gameplay rng 流

`src/game/systems/vfx_server.cpp` 5 个函数用全局 `rng()` 生成**纯视觉**粒子参数:
- `lightning` L38 (每 branch 1 draw)
- `explosion` L47-50 (**每 count 3 draws**)
- `smoke_puff` L66-67 (每 count 3 draws)
- `spark_burst` L72-75 (**每 count 3 draws**)
- `blood_frenzy` L259-264 (15+hit_count*3 次以上)

与 RNG-001 (`_add_noise` 偷吃主 rng 流, 7% 间歇分岔) **完全同族**:
RNG-001 修复时改了屏震 (game_scene.cpp:298) + 贴图噪声 (sprite_renderer.cpp:27),
**vfx_server.cpp 漏网**。一次攻击特效 (explosion(18)+spark_burst(8) 等) = 50~150
draws, 与 spike 观测精确吻合。

**间歇性解释**: VFX 掷骰本身确定 (同事件同消耗), 但**触发差异的上游源头**
仍指向堆布局敏感的未初始化读/指针比较 (P1-C4 手法: 输入一致输出分岔)。
分岔的第一信号 = 同 tick B 比 A 多走一次事件路径 (A +59 vs B +149),
之后 RNG 流永久错位 (B 后续每帧 draws 差 +12 漂移, F4+ 入层怪池/id 全分岔)。

### 9.4 修复实施 (2026-09-11, 方案 A)

**已实施**: vfx_server.cpp 5 个函数 11 处 `rng()` → `visual_rng()`
(lightning/explosion/smoke_puff/spark_burst/blood_frenzy) + 显式
`#include "combat_system.h"`。**探针已全部删除** (game_scene.cpp
FPDIAG/POSDIAG/SPDDIAG/RNGSPIKE/RNGSITE + combat_system.h call_sites
环形缓冲), 工作区回到干净修复 diff。

**验证结果**:
- 重建 Release: 0 errors
- ctest: **60/60 passed**
- world_validator: All checks passed
- **并行冒烟: 32/32 进程批 (16 对) 同哈希 `061A2BE6FBAB72D8366570C4416DBC1A`** —
  零分岔 (修复前同协议 12~50% 批次分岔, 每批 1~4 对分岔)
- 平衡基线: avg_floor=7.00 dmg_dealt=1987.2 (v1.4.14 旧基线 6.75/1823.4 同量级;
  旧基线本身被 RNG-002 污染, 轻微移动属预期 — **新基线以本次为准**)
- 证据文件: `reports/p1c8/fix/fix_*.out` (32 批, 勿删)

**关于"第一信号"**: 方案 A 消除了 RNG 流错位放大器。§9.3 所述"同 tick
B 多走一次事件路径"的上游触发差异, 在 32 批零分岔下未再现 — 可能它
本身就需要 RNG 污染才可观测 (VFX 掷骰是分岔的**必要环节**), 也可能
堆布局窗口极窄。**判定: 结案**; 若未来复现, 从本节 §9.1 方法论重启。

### 9.5 本节遗留疑问 (存档, 已不阻塞结案)

- A t=19.133 spike fl=4 vs POSDIAG fl=3: spike 探针读 game_time/current_floor
  在 tick 头、POSDIAG 在 tick 尾 — floor 切换帧二者差 1 属正常, 无碍结论
- run9 F3/F4 段边界 (draws 5.5万→6.2万) 与早前 run8 分析的窗口不同:
  rs_2 批分岔点是 run9, WIP §3 记录的是 run8 (批间分岔 run 不固定,
  但形态同构: 同 tick VFX 掷骰差 → 流错位 → 怪池雪崩)
