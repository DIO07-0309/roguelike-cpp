# B3 翻滚/闪避 实施计划 (v1.7-B3)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 玩家获得 Shift+方向 的纯手感翻滚位移（64px/0.16s/0.7s CD，无无敌帧），带 2D/3D 形变+残影+尘土表现，行为显式喂给 Mirror 采集，sim 行为零变化。

**Architecture:** 新 `DodgeComponent`（零 RNG、dt 定步长）组合进 `Player`；`PlayerController` 负责输入起翻与 tick 位移接管；渲染端（2D `Player::draw_no_cam` / 3D `_build_entities`+`_draw_billboard`）只读组件状态做表现。尘土走既有 `VFXServer→active_effects` 通道（2D/3D 双消费）。

**Tech Stack:** C++17, Raylib 5.0, GoogleTest, CMake。

**Spec:** `docs/superpowers/specs/2026-09-17-b3-dodge-roll-design.md`（实施勘误见 spec 末 §9）

## Global Constraints

- 函数 ≤40 行；类单一职责；组合优于继承；PascalCase 类 / camelCase 方法 / snake_case 变量
- 零新增依赖；不动 CMakeLists 编译标志；`.h` 用 `#pragma once`；禁 new（值成员/智能指针）
- 确定性红线：DodgeComponent 零 RNG、禁墙钟、dt 驱动；**sim 行为必须与改前逐字节一致**
- 一次生成 ≤300 行；新增中文文案后必须跑 `python tools/extract_chars.py` 重建字体码点
- 每任务结束 commit；JSON 未改动（尘土走直发原语），validator 只需 T5 全量跑

## 勘误（相对 spec，实现时执行勘误后条款）

1. 尘土**不改 vfx_recipes.json**：用既有直发模式 `VFXServer vfx; vfx.ring+spark_burst; effects 拷入 active_effects`（player_controller.cpp:470-473 同构）。3D 尘土经 active_effects→`_build_effects` 自动生效，**删** spec §4 的 A2 mote burst 专改。
2. 3D 不做倾斜旋转（`DrawBillboardRec` 不支持 rotation）；3D 表现 = squash 缩放 + 残影 quad，倾斜仅 2D（脚底 origin 现成）。
3. spec §6 用例"撞墙早停"归实机验收（`_try_move_axis` 二分贴墙是既有已验证逻辑，组件层 delta 恒定已由单测覆盖）。

---

### Task 1: DodgeComponent 纯逻辑 + 单测

**Files:**
- Create: `src/game/systems/dodge_component.h` / `.cpp`
- Create: `tests/combat/dodge_test.cpp`
- Modify: `tests/CMakeLists.txt`（在 `add_roguelike_test(weapon_test ...)` 附近加一行）

**Interfaces:**
- Produces: `DodgeComponent::{try_start(Vector2)->bool, tick(float), active()->bool, delta_this_frame()->Vector2, remaining_cd()->float, tilt_deg()->float, squash_scale()->Vector2, ghost_due()->bool, push_ghost(Vector2), ghosts()->vector<RollGhost>, reset()}`；常量 `kDistance=64,kDuration=0.16,kCooldown=0.7,kSpeed=400,kGhostLife=0.1f`；自由函数 `roll_direction(const Vector2&, Direction)->Vector2`

- [ ] **Step 1: 写 6 个失败用例**（fixture 直接栈上构造组件，无场景依赖）

```cpp
#include <gtest/gtest.h>
#include <cmath>
#include "systems/dodge_component.h"

TEST(DodgeComponent, RollMovesFullDistanceThenEnds) {
    DodgeComponent d; ASSERT_TRUE(d.try_start({1, 0}));
    float total = 0; for (int i = 0; i < 20 && d.active(); i++) { d.tick(1/60.f); total += d.delta_this_frame().x; }
    EXPECT_NEAR(total, DodgeComponent::kDistance, 8.0f);   // ±1 帧 (末帧 tick 终止不再计 delta)
    EXPECT_FALSE(d.active());
    EXPECT_NEAR(d.remaining_cd(), DodgeComponent::kCooldown - DodgeComponent::kDuration, 0.02f);
}
TEST(DodgeComponent, CooldownRejectsThenAllows) {
    DodgeComponent d; d.try_start({1, 0});
    for (int i = 0; i < 12; i++) d.tick(1/60.f);           // 0.2s: 翻滚已完, cd 还剩 0.5
    EXPECT_FALSE(d.try_start({1, 0}));
    for (int i = 0; i < 36; i++) d.tick(1/60.f);           // 累计 0.8s > cd
    EXPECT_TRUE(d.try_start({1, 0}));
}
TEST(DodgeComponent, RejectsWhileActive) {
    DodgeComponent d; d.try_start({0, 1}); d.tick(1/60.f);
    EXPECT_FALSE(d.try_start({0, 1}));
}
TEST(DodgeComponent, TiltSquashReturnToNeutral) {
    DodgeComponent d; d.try_start({1, 0}); d.tick(1/60.f); d.tick(1/60.f);
    EXPECT_NE(d.tilt_deg(), 0.0f);
    Vector2 s = d.squash_scale(); EXPECT_GT(s.x, 1.0f); EXPECT_LT(s.y, 1.0f);
    for (int i = 0; i < 20; i++) d.tick(1/60.f);
    EXPECT_FLOAT_EQ(d.tilt_deg(), 0.0f);
    Vector2 n = d.squash_scale(); EXPECT_FLOAT_EQ(n.x, 1.0f); EXPECT_FLOAT_EQ(n.y, 1.0f);
}
TEST(DodgeComponent, GhostCapAndDecay) {
    DodgeComponent d; d.try_start({1, 0});
    for (int i = 0; i < 4; i++) d.push_ghost({(float)i * 8, 0});
    ASSERT_EQ(d.ghosts().size(), 3u);                      // 上限 3, 留最新
    d.tick(0.2f);                                          // 全部超龄
    EXPECT_TRUE(d.ghosts().empty());
}
TEST(DodgeComponent, ResetClearsAll) {
    DodgeComponent d; d.try_start({1, 0}); d.tick(1/60.f); d.reset();
    EXPECT_FALSE(d.active()); EXPECT_FLOAT_EQ(d.remaining_cd(), 0.0f); EXPECT_TRUE(d.ghosts().empty());
    EXPECT_TRUE(d.try_start({1, 0}));                      // reset 后立即可翻
}
TEST(RollDirection, AxisPriorityAndFacingFallback) {
    Vector2 a = roll_direction({1, 1}, Direction::UP);     // 斜轴归一化
    EXPECT_NEAR(a.x, 0.707f, 0.01f); EXPECT_NEAR(a.y, 0.707f, 0.01f);
    Vector2 f = roll_direction({0, 0}, Direction::LEFT);   // 无输入 → 面向
    EXPECT_FLOAT_EQ(f.x, -1.0f); EXPECT_FLOAT_EQ(f.y, 0.0f);
}
```

- [ ] **Step 2: 运行确认编译失败**：`cmake --build build --t tests_...` 或直接 `cmake --build build && cd build && ctest -R Dodge` → 预期"找不到 dodge_component.h"

- [ ] **Step 3: 实现 .h/.cpp**

```cpp
// dodge_component.h
#pragma once
#include "raylib.h"
#include "entities/entity.h"   // Direction
#include <vector>

// B3: 翻滚纯手感组件 — 零 RNG / dt 定步长 / 无无敌帧 (spec §2)
struct RollGhost { Vector2 pos; float age; };

class DodgeComponent {
public:
    static constexpr float kDistance = 64.0f, kDuration = 0.16f, kCooldown = 0.70f;
    static constexpr float kSpeed = kDistance / kDuration;
    static constexpr float kGhostLife = 0.1f;

    bool try_start(const Vector2& dir_norm);
    void tick(float dt);                       // 推进计时/冷却/残影老化
    bool active() const { return _running; }
    Vector2 delta_this_frame() const {   // raylib.h 无 Vector2*float → 分量展开 (T1 实测)
        if (!_running) return {0.0f, 0.0f};
        float s = kSpeed * _dt;
        return {_dir.x * s, _dir.y * s};
    }
    float remaining_cd() const { return _cd > 0 ? _cd : 0; }
    float tilt_deg() const;
    Vector2 squash_scale() const;
    bool ghost_due() const { return _running && (_frame % 2 == 0); }
    void push_ghost(const Vector2& pos);       // 控制器补世界坐标 (rect 左上)
    const std::vector<RollGhost>& ghosts() const { return _ghosts; }
    void reset();

private:
    Vector2 _dir = {0.0f, 1.0f};
    bool  _running = false;
    float _elapsed = 0.0f, _cd = 0.0f, _dt = 0.0f;
    int   _frame = 0;
    std::vector<RollGhost> _ghosts;
};

Vector2 roll_direction(const Vector2& axis, Direction facing);
```

```cpp
// dodge_component.cpp
#include "systems/dodge_component.h"
#include <cmath>

bool DodgeComponent::try_start(const Vector2& dir_norm) {
    if (_running || _cd > 0.0f) return false;
    _dir = dir_norm; _running = true; _elapsed = 0.0f; _frame = 0; _cd = kCooldown;
    return true;
}
void DodgeComponent::tick(float dt) {
    _dt = dt;
    if (_cd > 0.0f) _cd -= dt;
    for (auto& g : _ghosts) g.age += dt;
    while (!_ghosts.empty() && _ghosts.front().age >= kGhostLife) _ghosts.erase(_ghosts.begin());
    if (!_running) return;
    _elapsed += dt; _frame++;
    if (_elapsed >= kDuration) { _running = false; _elapsed = 0.0f; _frame = 0; }
}
float DodgeComponent::tilt_deg() const {
    if (!_running) return 0.0f;
    float k = sinf(_elapsed / kDuration * 3.1415926f);
    float sign = _dir.x > 0.0f ? 1.0f : _dir.x < 0.0f ? -1.0f : 1.0f;
    return 12.0f * k * sign;
}
Vector2 DodgeComponent::squash_scale() const {
    if (!_running) return {1.0f, 1.0f};
    float k = sinf(_elapsed / kDuration * 3.1415926f);
    return {1.0f + 0.10f * k, 1.0f - 0.15f * k};
}
void DodgeComponent::push_ghost(const Vector2& pos) {
    if (_ghosts.size() >= 3) _ghosts.erase(_ghosts.begin());
    _ghosts.push_back({pos, 0.0f});
}
void DodgeComponent::reset() { *this = DodgeComponent{}; }

Vector2 roll_direction(const Vector2& axis, Direction facing) {
    float len = sqrtf(axis.x * axis.x + axis.y * axis.y);
    if (len > 0.1f) return {axis.x / len, axis.y / len};
    switch (facing) {
        case Direction::UP:    return {0.0f, -1.0f};
        case Direction::DOWN:  return {0.0f,  1.0f};
        case Direction::LEFT:  return {-1.0f, 0.0f};
        case Direction::RIGHT: return { 1.0f, 0.0f};
    }
    return {0.0f, 1.0f};
}
```

- [ ] **Step 4: 注册 + 跑绿**：tests/CMakeLists.txt 加 `add_roguelike_test(dodge_test combat/dodge_test.cpp)`；`cmake -B build -DENABLE_TESTS=ON` 重配 → `cmake --build build` → `ctest` → 预期 **62/62**

- [ ] **Step 5: Commit** `feat: B3-T1 DodgeComponent 纯逻辑组件 + 6 单测 (零RNG/dt定步长)`

---

### Task 2: 输入注册 + Player 组合 + 控制器接线（2D 可玩）

**Files:**
- Modify: `src/core/input_map.cpp:46-62`（setup_defaults）
- Modify: `src/game/entities/player.h`（成员区 ~98-110）、`src/game/entities/player.cpp:58-68`（reset_attack_timers）
- Modify: `src/game/player_controller.h`（私有 helper 声明）、`src/game/player_controller.cpp`（handle_input ~350 后；tick 130-169 区）

**Interfaces:**
- Consumes: T1 的 DodgeComponent 全部接口；既有 `_is_action_just_pressed`、`_try_move_axis`、`g_behavior.on_dodge(time,floor,px,py)`、`input.get_movement_axis()`
- Produces: 键位动作 `"dodge"`；`Player::dodge` 公共成员；`PlayerController::_try_start_dodge/_roll_dust`（T3 用 `_roll_dust`）

- [ ] **Step 1: input_map 注册**（add_action 对同动作 append 多键，_bindings 是 vector）

```cpp
    add_action("dodge", KeyboardKey::LEFT_SHIFT);
    add_action("dodge", KeyboardKey::RIGHT_SHIFT);   // B3: 翻滚 (任一 Shift)
```

- [ ] **Step 2: Player 组合成员**：player.h include 区加 `#include "systems/dodge_component.h"  // B3: 翻滚组件`；成员区（`ComboState combo;` 旁）加：

```cpp
    // B3: 翻滚组件 (输入驱动, 独立冷却, 不碰武器 recovery)
    DodgeComponent dodge;
```

player.cpp `reset_attack_timers()` 尾部加 `dodge.reset();  // B3: 换层清翻滚态/冷却`

- [ ] **Step 3: handle_input 起翻分支**（插在 `player_controller.cpp:347-349` 镜像冻结 `return` 之后、attack 分支之前）：

```cpp
    // B3: 翻滚 — UI 早退链与镜像冻结门已被上方 return 覆盖
    if (gs._is_action_just_pressed(input, "dodge"))
        _try_start_dodge(gs, input);
```

player_controller.h 私有区加声明；.cpp 实现（≤40 行）。**本任务内 `_roll_dust` 先定义为空体** `void PlayerController::_roll_dust(GameScene&, float, float) {}`（T3 填充），保证 T2 独立可编译可提交：

```cpp
void PlayerController::_try_start_dodge(GameScene& gs, const InputMap& input) {
    auto& dg = gs.player->dodge;
    Vector2 dir = roll_direction(input.get_movement_axis(), gs.player->direction);
    if (!dg.try_start(dir)) return;
    const auto& r = gs.player->entity.rect;
    float cx = r.x + r.width / 2, cy = r.y + r.height / 2;
    g_behavior.on_dodge((float)gs.game_time, gs.current_floor, cx, cy);
    _roll_dust(gs, cx, cy);   // T2 空体 / T3 实装
}
```

- [ ] **Step 4: tick 位移接管**（`player_controller.cpp` 130 门内、`_record_move` 之后）。先把 152-167 的 `_try_move_axis` lambda 原样上移到 145 `auto& e` 之后，再改分支：

```cpp
        _record_move(move.x, move.y);
        bool was_dodging = gs.player->dodge.active();
        gs.player->dodge.tick(dt);                       // B3: 定步长推进
        if (gs.player->dodge.active()) {
            Vector2 d = gs.player->dodge.delta_this_frame();
            _try_move_axis(true, d.x);
            _try_move_axis(false, d.y);                  // 撞墙=既有二分贴墙, 计时照跑
            if (gs.player->dodge.ghost_due())
                gs.player->dodge.push_ghost({e.position.x, e.position.y});
        } else if (was_dodging) {
            const auto& r = e.rect;
            _roll_dust(gs, r.x + r.width / 2, r.y + r.height / 2);   // 落地尘 (T3 填充)
        } else {
            float speed_mul = (gs._tw_speed_boost > 0) ? 1.25f : 1.0f;
            float s = get_effective_speed(gs.player.get()) * speed_mul * dt;
            _try_move_axis(true,  move.x * s);
            _try_move_axis(false, move.y * s);
        }
```

（147-148 原 `speed_mul/s` 两行并入 else；其余原样）

- [ ] **Step 5: sim 红线双验证**：改前先跑基线存档，改后对比：

```powershell
cmake --build build
# 改后基线 (seed 3 x12) 与改前报告逐字节:
& build\roguelike_cpp.exe --sim 12 --sim-seed 3 2>$null | Out-Null
Compare-Object (Get-Content "$env:TEMP\opencode\pre_b3_report.json") (Get-Content reports\balance_report.json)
```

预期：diff 空（Agent 无 dodge 动作 → 组件永不 active）。`cd build; ctest` → 62/62

- [ ] **Step 6: Commit** `feat: B3-T2 翻滚接线 — Shift 起翻/tick 位移接管/on_dodge 采集/sim 零变化`

---

### Task 3: 2D 表现层 — 倾斜压扁 + 残影 + 尘土

**Files:**
- Modify: `src/game/entities/player.cpp:290-346`（draw_no_cam）
- Modify: `src/game/player_controller.cpp`（`_roll_dust` 实装；若 T2 留空）

**Interfaces:**
- Consumes: `dodge.tilt_deg()/squash_scale()/ghosts()`、`VFXServer::ring(x,y,r,color,count,dur)`、`spark_burst(x,y,n,color,dur)`（player_controller.cpp:471 同款）
- Produces: 2D 完整翻滚表现（3D 的在 T4）

- [ ] **Step 1: _roll_dust 实装**（直发原语 → active_effects，2D/3D 双消费）

```cpp
static void _roll_dust_impl(GameScene& gs, float cx, float cy) {
    VFXServer vfx;
    vfx.ring(cx, cy, 22.0f, {190, 180, 165, 170}, 2, 0.28f);
    vfx.spark_burst(cx, cy, 4, {210, 200, 185, 200}, 0.25f);
    for (auto& e : vfx.effects) gs.active_effects.push_back(e);
    vfx.effects.clear();
}
void PlayerController::_roll_dust(GameScene& gs, float cx, float cy) { _roll_dust_impl(gs, cx, cy); }
```

- [ ] **Step 2: draw_no_cam 形变**——`player.cpp:301` 改为（hx/hy 居中补偿行原样）：

```cpp
    Vector2 sq = dodge.squash_scale();   // B3: 翻滚压扁 (默认 1,1 零影响)
    float hw = dr.width * heavy_scale * sq.x, hh = dr.height * heavy_scale * sq.y;
```

`:326-327` rot 合成（脚底 origin 回弹路径现成复用）：

```cpp
        float rot = ((combo.is_heavy() && p > 0.0f) ? 6.0f * sinf(p * 6.2831853f) : 0.0f)
                    + dodge.tilt_deg();  // B3: 翻滚倾斜叠加
```

- [ ] **Step 3: 残影绘制**——`ftex.id > 0` 块内、主 DrawTexturePro 之后加：

```cpp
        // B3: 残影 — 同贴图, alpha 随年龄衰减 (pos 记录起翻时 rect 左上)
        for (const auto& g : dodge.ghosts()) {
            float a = 120.0f * (1.0f - g.age / DodgeComponent::kGhostLife);
            if (a <= 0.0f) continue;
            Rectangle gd = {dr.x + (g.pos.x - entity.position.x),
                            dr.y + (g.pos.y - entity.position.y),
                            dr.width, dr.height};
            DrawTexturePro(ftex, src, gd, {0, 0}, 0, {255, 255, 255, (unsigned char)a});
        }
```

- [ ] **Step 4: 验证**：`cmake --build build` 0 error；ctest 62/62；`--hd2d` 关闭的 2D hidwin autoshot 冒烟（不按键 → 画面应与基线一致，PIL 均值亮度 diff <1）。实机 Shift 手感归 T5 用户验收。Commit `feat: B3-T3 2D 翻滚表现 — 脚底倾斜/压扁回弹/3 段残影/尘土`

---

### Task 4: 3D 表现层 — billboard squash + 残影 quad

**Files:**
- Modify: `src/game/rendering3d/hd2d_renderer.h`（HD2DDrawItem）
- Modify: `src/game/rendering3d/hd2d_renderer.cpp:536-572`（_draw_billboard）
- Modify: `src/game/rendering3d/hd2d_scene_builder.cpp:401-421`（_build_entities 玩家块）

**Interfaces:**
- Consumes: `Player::dodge` 只读查询；既有 ENTITY_BILLBOARD/`DrawBillboardRec`/painter 排序（sort_y）
- Produces: `HD2DDrawItem::scale_w/scale_h`（默认 1 → 全存量行为不变）

- [ ] **Step 1: 字段**：HD2DDrawItem `Color top_tint = {};` 行后加：

```cpp
    float scale_w = 1.0f, scale_h = 1.0f;   // B3: billboard 形变 (1,1 = 原行为)
```

- [ ] **Step 2: _draw_billboard 应用**：`float w = item.size;` 起两行改为：

```cpp
    float w = item.size * item.scale_w;
    float h = item.size * 1.5f * item.scale_h;
```

- [ ] **Step 3: builder 玩家块**（`out.push_back(item)` 之前）：

```cpp
        Vector2 sq = gs.player->dodge.squash_scale();
        item.scale_w = sq.x; item.scale_h = sq.y;   // B3: 翻滚形变 (tilt 见勘误2: 3D 不做)
        for (const auto& g : gs.player->dodge.ghosts()) {   // 残影先入 → painter 稳定序垫底
            float a = 120.0f * (1.0f - g.age / DodgeComponent::kGhostLife);
            if (a <= 0.0f) continue;
            HD2DDrawItem gh = item;
            gh.world_pos = {g.pos.x + r.width * 0.5f, 0, g.pos.y + r.height * 0.5f};
            gh.tint = {255, 255, 255, (unsigned char)a};
            gh.outline = false;
            out.push_back(gh);
        }
```

（builder include `systems/dodge_component.h` 若经 player.h 已可见则免）

- [ ] **Step 4: 验证**：build 0 error；ctest 62/62；hidwin `--hd2d --goto-floor 6 --autoshot` 冒烟（不按键，回归比对）；sim 双跑字节一致复测（3D 不进 sim，形式合规）。Commit `feat: B3-T4 3D 翻滚表现 — billboard squash + 残影 quad (HD2DDrawItem.scale_*)`

---

### Task 5: 文档 + 码点 + 全量门禁 + 桌面包

**Files:**
- Modify: `README.md`（操作表 ~52 行区）、`src/game/scenes/title_scene.cpp:454` 区、`tutorial_scene.cpp:136` 区（若有键位文案）
- Modify: `CHANGELOG.md`、`docs/V1_6_ROADMAP.md`（B3 行 ✅）、spec §9 勘误回写（若尚未）

- [ ] **Step 1: 文案**。README 操作表加 `| **Shift** | 翻滚闪避（按住方向键定翻滚方向） |`；title_scene 帮助行（`"E - 交互   B - 背包   F1 - 日志",` 同风格）加一行 `"Shift - 翻滚",`（DrawTextCH 链路已有）；查 tutorial_scene 是否列键位，有则同款加行。

- [ ] **Step 2: 码点**：`conda run python tools/extract_chars.py` → 若 1936 增长则重建字体 atlas（沿 B 线流程），确认"翻/滚/闪/避"在内。

- [ ] **Step 3: 全量门禁**：Release 0 error · `cmake --build build`+`ctest` 62/62 · `conda run python tools/world_validator.py` 0/0 · sim 双跑字节一致 + 对 `pre_b3_report.json` diff 空 · hidwin 2D/3D 冒烟回归。

- [ ] **Step 4: CHANGELOG**（`# v1.7-B3 — 翻滚/闪避: 纯手感位移 + 表现全套 (2026-09-17)` 条目：机制参数、Mirror 采集接入、sim 零变化声明、勘误 3 条、门禁结果）；roadmap B3 行改 `| B3 ✅ v1.7-B3 | 翻滚/闪避 ... | 完成 |`。

- [ ] **Step 5: Code Review 自查**（规则 6）：函数行数全 ≤40（_try_start_dodge≈10 / tick 新增分支≈14 / draw 改动≈8）；无 new；无 using namespace std 入 .h；dodge_component 无墙钟/GetTime 调用；确认 commit。

- [ ] **Step 6: 桌面包同步**（规则 11）：src/docs/resources(未变)/tools 镜像 + 根文件 + **exe 到根目录**；路径用无空格 `C:\Users\HP\Desktop\Roguelike-CPP-3D版`。

- [ ] **Step 7: Commit** `docs: B3-T5 README/帮助页/CHANGELOG/roadmap 收口 + 门禁记录`

---

## 验收移交（完成后向用户汇报）

实机点检验收：Shift 翻滚手感（起/落地/撞墙贴停）、残影拖尾观感、尘土、Mirror HUD 的 DODGE 计数开始增长；若不满意倾斜角度/压扁幅度/时长冷却，参数全在 dodge_component.h 顶部 constexpr，一行可调。


---

## 实施勘误 (T1-T5 实测后, 以代码为准)

1. **T2 delta**: 最终实现 delta_this_frame() 返回 {_dir * kSpeed * _move_t};
   _move_t = 本帧有效位移秒时长, 末帧余量补足 → 总位移精确 64px。
2. **T2 tick**: _try_move_axis lambda 上提, 翻滚全程(含落地帧)走单一
   if (was_dodging) 分支; 落地尘在该分支末触发。
3. **ctest 粒度**: 按可执行注册 → 61→62/62 (spec SS9-5), 非 67。
4. **T5 title 面板**: 循环由硬编码 i<6 改 sizeof 自适应; 布局 y70/行距19/高155
   避让右侧怪物队列遮挡线 (~y218)。
5. **冒烟取图**: 字体 atlas 生成 ~1s, --autoshot 120 快进下早拍 → 用 3600 帧。
