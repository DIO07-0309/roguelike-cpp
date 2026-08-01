# VFX 特效系统审查报告 (2026-08-01)

> 审查范围：`src/game/systems/vfx_server.cpp/h`、`src/game/systems/game_renderer.cpp`、`src/data/vfx_recipe.cpp/h`、`resources/vfx_recipes.json`、`src/game/types/combat_types.h`、调用链（`game_scene.cpp` / `combat_coordinator.cpp` / `player_controller.cpp`）

## 1. 系统架构

```
技能/怪物/Boss → VFXServer primitives (10种) 或 play_recipe(JSON 配方)
       → std::vector<Effect> → 拷贝进 game_scene.active_effects
       → GameRenderer::draw_effects 渲染
```

- 图元：ring / beam(bolt) / lightning / explosion / shockwave / slash_arc / smoke / spark / aura(shield_ring) / flash
- 配方：`resources/vfx_recipes.json` 12 个，支持 `layers`/`layer_delay`/`delay` 分镜
- 唯一渲染入口：`GameRenderer::draw_effects`（game_scene.cpp:1238）

## 2. 已修复（G5.8.8-fix，本轮已完成）

| # | 问题 | 状态 |
|---|------|------|
| F1 | 加载器要求数组根，JSON 实际是 object → 加载必然失败，全部配方走 fallback | ✅ 已修：兼容两种根 |
| F2 | JSON step 字段 `layers`/`layer_delay`/`delay` 未解析 | ✅ 已修：VFXStep 补 3 字段 |
| F3 | `play_recipe` 不支持 `cone`/`bolt` kind；ring 层数误用 count | ✅ 已修：cone→slash_arc 兜底、bolt→beam、ring 用 layers |
| F4 | 分镜 delay 无实现（JSON 注释说走 Timeline，系统不存在） | ✅ 已修：Effect 加 `start_delay`，更新/渲染双入口同步 |

## 3. 待修复问题

> 状态更新 (2026-08-01)：P1–P4 已全部修复并验证，见下方 ✅ 标记。

### P1 ✅ 已修：ring/smoke/aura 特效在游戏中不可见

**定位**
- `VFXServer::draw`（vfx_server.cpp:16-56）实现完整渲染（含 ring/shield_ring/smoke），但**无任何调用点**（全仓库 grep 确认）
- 实际渲染入口 `GameRenderer::draw_effects`（game_renderer.cpp:66-105）只有分支：`pulse/spark/bolt/flash/slash_arc/cone`，**没有 else 兜底**，也不含 `ring`/`smoke`/`shield_ring`
- 所有 `ring()`/`smoke_puff()`/`aura_ring()`/`explosion()`（内部调 ring）生成的特效类型在 draw_effects 中落空 → **不可见**

**修复（已完成）**
1. `game_renderer.cpp` 重写 `draw_effects`：提取 `_draw_effect_body()`（全 kind 分支，含 ring/smoke/shield_ring/generic 兜底）+ `_draw_slash_arc()`，符合 40 行规范
2. 删除 `VFXServer::update/draw` 死代码（头文件 + cpp）
3. 顺带清理 `SceneTree::_vfx` 空转全局容器（`get_vfx()` 无调用者、`_vfx` 从未被塞入特效，update/draw 每帧空跑）——删除成员、`get_vfx()`、`_render()`、相关 include

### P2 ✅ 已修：8 个技能无专属特效

**修复（已完成，纯 JSON）**
`vfx_recipes.json` 补 10 个配方（`skill_` 前缀与 play_recipe 解析链匹配）：
| recipe id | 设计 |
|---|---|
| `skill_the_world` | time 色：flash + 3 层脉冲 + 18 spark（时停大招演出） |
| `skill_meteor` | fire 色：bolt 下落 + shockwave + 3 层脉冲 + 16 spark + flash，震屏 6 |
| `skill_blood_slash` | bleed 色 slash_arc + spark + cone |
| `skill_frost_edge` | ice 色 slash_arc + spark + cone |
| `skill_shadow_slice` | shadow 色 slash_arc + 烟 + flash + spark |
| `skill_self_heal` | heal 色 pulse + spark |
| `skill_blood_pact` | bleed 脉冲 + heal spark + flash |
| `skill_blizzard_ward` | ice 色 3 层脉冲 + spark + flash |
| `skill_blood_frenzy` | bleed 色双脉冲 + 20 spark + flash，震屏 4 |
| `skill_summon_spirit` | summon 色 3 层脉冲 + spark + flash |

顺带 `time_stop` 配方加 `"color": "time"`（原为默认金色）。

验证：22/22 配方加载成功，全部 kind 合法（12 种），world_validator 0 错。

### P3 ✅ 已修：`cone` 分支死代码

`_emit_step` 中 `cone` 由"映射 slash_arc"改为直接生成 `"cone"` kind Effect，复用 game_renderer.cpp 的矩形锥形渲染。`skill_slash` 第 3 步（delay 0.15）现在显示真实锥形。

### P4 优化

| # | 问题 | 状态 |
|---|------|------|
| P4a | `_emit_step` 中 `bolt` 与 `beam` 分支代码完全重复 | ✅ 已修：合并为一个分支 |
| P4b | `time_stop` 配方无 `"color"` 字段 → 默认金色 | ✅ 已修：`"color": "time"` |
| P4c | JSON sfx 字段 `ice_crack`/`lightning`/`summon` 在 AudioServer 不存在 | ⏳ 待接线时处理（当前无实际播放） |
| P4d | `sfx`/`hit_sfx`/`camera_shake` 已解析未接线（play_recipe 无 audio/相机参数） | ⏳ 建议单独排期：play_recipe 增加 AudioServer* 参数 + 震屏由调用方读取 recipe 字段 |

## 4. 修复结果

- [x] P1 渲染不可见
- [x] P2 技能配方补全（10 个）
- [x] P3 cone 独立渲染
- [x] P4a/P4b 小优化
- [ ] P4c/P4d sfx 接线（新功能，另行排期）
