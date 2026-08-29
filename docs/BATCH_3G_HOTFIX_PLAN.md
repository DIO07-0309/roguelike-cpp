# Batch 3G: Hotfix Plan — Font / Room Placement / Fullscreen

## Issue 1: Chinese Font "？" Display

### Root Cause
`font_codepoints.h` 缺少 68 个 CJK 字符（含 钥/匙/售/进化 等）。已在 `3a9c13b` 修复（1769→1837 码点）。

### Status
**已修复**。`extract_chars.py` 重跑 + 重编译后 Raylib `LoadFontEx()` 会构建包含全部 1837 码点的字体图集。

### Action
- 无需额外代码改动
- 需确认桌面 exe 是最新 build（已同步）

---

## Issue 2: Gamble Room & Challenge Room Placement

### Root Cause
- **Gamble Room**: 旧 `idx%9` 循环 + 洗牌池修复后类型可达，但位置随机（偏向 Room 1），无位置保证
- **Challenge Room**: 已在出口附近，但需确认不阻塞主路径

### Plan

#### 2.1 Gamble Room — 固定出生侧
**文件**: `dungeon_generator.cpp` `_assign_special_rooms()`

当前逻辑：从洗牌池随机分配类型到候选房间。

修改：
1. 在 `_assign_special_rooms` 中，遍历 candidates 时优先检查：如果当前类型是 `GAMBLER`，则**强制分配到 Room 1**（或第一个合格的非特殊房间）
2. 排除条件：Boss 房（rooms.back()）、出口/楼梯房、已有特殊房间
3. 如果 Room 1 已被占用，找第一个合格的靠近出生的房间

```
candidates 逻辑保持不变（Room 1 优先）
洗牌池分配时：GAMBLER 类型 → 强制放到 candidates[0]（Room 1）
其他类型 → 正常按洗牌池顺序分配
```

#### 2.2 Challenge Room — 出口侧 + 路径安全
**文件**: `dungeon_generator.cpp` challenge room 放置逻辑（line 41-72）

当前逻辑：已选择距离出口最近的未分配房间。

修改：
1. 添加路径安全检查：确认 Challenge Room 不在出生点到出口的**唯一必经通路**上
2. 实现方式：从出生房到出口房做 BFS，如果 Challenge Room 占据了 BFS 路径上的关键节点（割点），则跳过该房间，选下一个最近的
3. Boss 层（special_room_count=0）已自动跳过，无需额外处理

#### 2.3 Regression Tests
**文件**: `tests/world/challenge_room_test.cpp`（扩展）

新增测试：
- `GambleRoomNearSpawn`: 同 seed 多次生成，Gamble Room 始终在 Room 1 附近
- `ChallengeRoomNearExit`: Challenge Room 始终在出口附近
- `ChallengeRoomNotBlockingPath`: Challenge Room 不阻塞出生→出口路径
- `BossFloorNoChallenge`: Boss 层（5/10/15）不生成 Challenge Room
- `DeterministicPlacement`: 同 seed → 同房间位置

---

## Issue 3: Fullscreen Scaling

### Root Cause
`ToggleFullscreen()` 只切换全屏模式，不检测显示器分辨率，不缩放。960×640 画面居中显示，四周黑边。

### Plan
**文件**: `src/game/systems/game_renderer.cpp` draw() 调用链 + `src/main.cpp`

实现等比缩放居中（letterbox/pillarbox）：

1. 全屏时获取显示器分辨率 `GetMonitorWidth()/GetMonitorHeight()`
2. 计算等比缩放比例 `scale = min(monitor_w/960, monitor_h/640)`
3. 计算偏移量 `offset_x = (monitor_w - 960*scale)/2`, `offset_y = (monitor_h - 640*scale)/2`
4. 使用 Raylib 的 `BeginTextureMode()` + `DrawTexturePro()` 或直接 `rlViewport()` + `rlMatrixModeProjection()` 实现缩放
5. 推荐方案：使用 `Camera2D` + `BeginMode2D()` + `EndMode2D()` 的缩放模式，或直接在渲染管线末尾做一次全屏 blit

**具体实现**（推荐最小侵入方案）：
```cpp
// main.cpp 或 game_renderer.cpp 的 render 循环
if (IsWindowFullscreen()) {
    int mw = GetMonitorWidth(0);
    int mh = GetMonitorHeight(0);
    float scale = fminf((float)mw / WINDOW_WIDTH, (float)mh / WINDOW_HEIGHT);
    Rectangle src = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
    Rectangle dst = {
        (mw - WINDOW_WIDTH * scale) / 2,
        (mh - WINDOW_HEIGHT * scale) / 2,
        WINDOW_WIDTH * scale,
        WINDOW_HEIGHT * scale
    };
    // 将当前帧缓冲绘制到全屏窗口，等比缩放居中
}
```

或者更简洁：使用 `SetWindowSize()` 在全屏时把窗口设为显示器分辨率，然后在 SceneTree 的渲染中用 `rlViewport` 裁剪到正确比例。

---

## Implementation Order

| Step | Files | Lines |
|------|-------|-------|
| 1 | `dungeon_generator.cpp` | Rewrite `_assign_special_rooms` + challenge room placement |
| 2 | `dungeon_generator.cpp` + `scene_tree.cpp` / `main.cpp` | Fullscreen scaling |
| 3 | `tests/world/` | Add regression tests |
| 4 | Build + ctest + sync desktop |

## Constraints
- 函数 ≤40 行
- 组合优于继承
- 每次改动前 grep 所有引用点
- 不引入新第三方库
