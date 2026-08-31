# P0 Runtime Investigation — Sim "Permanent F1 Stuck" 死锁

> 批次: P0-M1 (Investigation) + P0-M2 (Minimal Fix)
> 日期: 2026-08-31
> 状态: **M2 已完成 — 死锁已消除；SimAI 导航质量遗留为已知项**

---

## 1. 症状（初始误判：疑似 heap corruption）

同一 exe 同一命令，结果在两种状态间**无规律摆动**：
- 全部局 900s 超时、avg_floor=1、怪满血、攻击≈0
- 或正常推进 avg_floor 2-5

历史包袱：Q3.11 提交曾记录"间歇堆损坏崩溃 ntdll"，先入为主怀疑内存损坏。
**误判纠正**：Start-Process 0xC0000005 复现实验实际是**缺 MinGW 运行时 DLL**
（0xC0000135，诊断构建用 dbghelp 后 PATH 未含 mingw64\bin）。非内存错。

## 2. 真正根因链（Trigger Amplification，非单点引入）

bisect 显示 v1.2.3 (f2e1f29) 是"首个坏点"——但它是**放大边界**而非 bug 引入点：

```
v1.2.3 怪物房间边界 (monster_room != player_room → IDLE)   [设计正确]
        ×
Room Encounter 进房锁门 (lock_room_doors → LOCKED)         [设计正确]
        ×
SimAI 旧传送 (_teleport_player_to_nearest 任意 8 邻格落点)
        ↓
传送落点在门口/走廊 tile (room_at = -1)
        ↓
目标怪判定跨房 → IDLE 永久冻结
        ↓
玩家攻击圈永远够不到 → 8s 卡死检测触发 → 再传 → 死循环
        ↓
900s/局 × 10 局 = 整批 sim 卡 F1
```

不同 seed 房间拓扑不同 → 触发概率不同 → 表现"随机"。

**第二个放大点**：卡死检测 `tile 未变` 判定被"墙前两 tile 来回震荡"永久重置
（BFS 路径记忆 L/R 横跳，tile 每次都变）→ 传送永不触发 → 纯震荡 896s。

## 3. P0-M2 修复内容

**A. 传送目标房间归属确定性**（`src/core/sim/sim_ai_teleport.h/.cpp` 新增）
- 纯函数 `sim_ai_teleport_target()`，分层候选：
  - Tier 1: 同房 r=2 环（攻击距离 ~64px，不贴脸围殴）
  - Tier 2: 同房任意合法格（小房间）
  - Tier 3: 任意可走格（最后手段）
- 排除：墙 / LOCKED·SEALED 门 / 与活怪 rect 重叠
- 契约测试：`tests/sim/p0_teleport_test.cpp` 3 case
  - LockedEncounterTeleportSameRoom / TeleportAvoidsInvalidTarget /
    NoSameRoomTileFallbackStillWalkable

**B. 卡死检测锚点半径化**（sim_ai.cpp stuck 判定）
- 原: 同 tile 才计时 → 被两 tile 震荡永久重置
- 新: 距锚点 ≤2 tile 徘徊即计卡死；传送后重锚

**C. DecisionAgent::set_room_manager()**（GameScene 每局注入 room domain）

## 4. 验证结果

| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 900s 死锁局 | 频繁（~100%） | **0** |
| 同 seed 可重复 | 否（结果漂移） | **是（log MD5 逐字节一致）** |
| ctest | 59/59 | **60/60**（+p0_teleport_test） |
| avg_floor | 1 | 1-2（见 §5） |

## 5. 遗留已知项（明确不在 P0-M2 范围）

死锁消除后 sim 玩家在 F1 **快速正常死亡**（80-200s/局，非卡死）。
数据画像：
- 传送进房 → attack 正常发生 → 打 1-2 只 → 被 poison stacks + 兽人围殴耗死
- 或: 清完一房后 BFS/搜刮分支把玩家引向下一房途中反复"传送-脱离"拉锯

根因属 **SimAI 导航策略质量**（_evaluate_move 搜刮分支权重、路径记忆震荡、
fist DPS vs F1 怪密度平衡），非空间死锁。**修复它们需要 500 局平衡回归**，
属于 SimAI 调优批次（建议 P1），不应在 P0 顺手改。

## 6. 契约（未来红线）

- `sim_ai_teleport_target` 是传送目标唯一来源，禁止旁路手写落点
- 传送后 player_room == target monster_room（Tier1/2 保证）
- 同 seed sim 必须逐字节可重复（CI 可加 hash 断言）
