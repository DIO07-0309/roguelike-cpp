# Batch 3H: Hotfix Plan — Fullscreen / PD Bug / Gamble UI / Inventory Keys

## Issue 1: Fullscreen Rendering Jitter

### Root Cause
`rlOrtho` + `rlViewport` 方案根本性错误：`_render()` 在一个调用中同时绘制世界空间（需要缩放 viewport）和 HUD 空间（需要全屏 viewport），无法分离。

- 摄像机居中使用显示器分辨率（1920）而非逻辑分辨率（960），偏移 480px
- HUD 坐标用 1920 计算但投影到 960 宽的 ortho → 超出可视区域
- "restore" 代码在所有绘制完成后执行 → 死代码

### Fix: RenderTexture2D 方案
**文件**: `src/core/scene_tree.cpp` + `src/game/scenes/game_scene.cpp`

1. 在 SceneTree 中创建 `RenderTexture2D target = LoadRenderTexture(960, 640)`
2. `_render()` 始终渲染到 `target`（960×640 逻辑坐标）
3. `EndDrawing()` 前，将 `target` 绘制到屏幕：
   - 窗口模式：直接 `DrawTextureRec(target, ...)` 全屏绘制
   - 全屏模式：等比缩放 + 居中 letterbox/pillarbox
4. 游戏内所有 `get_width()/get_height()` 改为始终返回 960/640（逻辑分辨率）

**优点**：
- 零侵入：所有现有绘制代码无需修改
- 世界和 HUD 都在 960×640 逻辑坐标中工作
- 缩放完全由最终 blit 控制

**修改文件**：
- `src/core/scene_tree.h`：添加 `_target` 成员（RenderTexture2D）
- `src/core/scene_tree.cpp`：run() 中用 BeginTextureMode/EndTextureMode + DrawTextureRec
- `src/core/scene_tree.h`：`width()/height()` 始终返回960/640（逻辑分辨率）

---

## Issue 2: PD 每帧叠加 Bug

### Root Cause
`RelicEffectProcessor::_apply_passive_stat()` 每帧直接 `+=` 修改 `physical_defense`，无一次性保护。60FPS 下 iron_ring(value=5) 每秒 +300 PD。

### Fix
**文件**: `src/game/systems/relic_effect_processor.cpp`

方案：PASSIVE 效果改为**仅在进入楼层时应用一次**，离开楼层时移除。在 Player 上增加 `int passive_pdef_bonus` 字段跟踪已应用的加成，避免重复叠加。

或者更简单：`_apply_passive_stat` 不再直接修改 base stat，改为累加到一个 `modifiers` map 中，由 `get_effective_defense()` 读取。

---

## Issue 3: 赌徒房抽奖界面

### Design
**状态机**: IDLE → SHOW_ODDS → SPINNING → RESULT → back to IDLE

**交互流程**:
1. 玩家按 E → 打开抽奖界面（非一次性消耗）
2. 界面显示：当前金币、奖池概率、[E]抽奖(20金)、[ESC]退出
3. 按 E → 扣20金 → 播放滚动动画（0.5s）→ 显示结果 → 回到步骤2
4. 金币不足 → 显示"金币不足"→ 回到步骤2

**奖池**（20金固定价格）：
- 65% 装备（随机稀有度）
- 20% 钥匙×1
- 10% 金币返还×10
- 5% RUN 圣物

**修改文件**：
- `src/game/world/special_room.h`：添加 `GambleUIPhase` enum + `GambleUIState` struct
- `src/game/world/special_room.cpp`：重写 `_exec_gambler()` 为状态机
- `src/game/systems/game_renderer.cpp`：添加 `draw_gamble_panel()` 方法
- `src/game/scene/game_scene.h`：添加 `gamble_open` / `gamble_cursor` / `gamble_state`
- `src/game/scene/game_scene_interaction.cpp`：添加 gamble UI tick/draw
- `src/game/player_controller.cpp`：E 键在赌徒房打开 UI 而非直接消耗

---

## Issue 4: 背包显示钥匙数

### Fix
**文件**: `src/game/systems/game_renderer.cpp` `draw_inventory_panel()`

在金币显示旁边添加钥匙显示：
```
金币: 125  钥匙: 3
```

---

## Implementation Order

| Step | Files | Description |
|------|-------|-------------|
| 1 | `scene_tree.h/cpp` | RenderTexture2D fullscreen fix |
| 2 | `relic_effect_processor.cpp` + `player.h` | PD bug fix |
| 3 | `special_room.h/cpp` + `game_renderer.cpp` + `game_scene*.cpp` + `player_controller.cpp` | Gamble UI |
| 4 | `game_renderer.cpp` | Inventory keys display |
| 5 | Build + ctest + sync desktop |

## Constraints
- 函数 ≤40 行
- 组合优于继承
- 不引入新第三方库
