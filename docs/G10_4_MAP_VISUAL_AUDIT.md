# G10.4-A — 地图视觉审计与方案设计 (Design Gate)

> 状态: **待 Review — 未批准前不写代码**
> 日期: 2026-08-30
> 前置: G10.3 Visual Slice Review（HUD ✅ / Monster ✅ / VFX ⚠️ / Map ⚠️）
> 本文档只回答: 地图为什么单调 / 根因排序 / 最小改动方案

---

## 1. 核心结论（先说答案）

**地图单调不是资产不足，而是"已有基础设施被绕过 + 素材本身是纯色"。**

三大根因（按影响排序）：

| # | 根因 | 位置 | 严重度 |
|---|------|------|--------|
| 1 | **生物群系调色板被完全绕过** — 纹理路径 tint 恒为白色，3 个群系看起来一模一样 | `game_map.cpp:295,369` | 🔴 主因 |
| 2 | **floor.png 是纯色图** — 16×16 只有 1 种颜色，零内部结构 | `assets/sprites/floor.png` | 🔴 主因 |
| 3 | **零 per-tile 变体** — 每个 FLOOR tile 画同一张纹理，无哈希变化 | `game_map.cpp:365-373` | 🟡 次因 |

---

## 2. 证据链

### 根因 1: 调色板死代码（最讽刺的发现）

**群系配色已完整定义**（`resources/biomes.json`，每个群系 11 色）：
- 监狱 F1-5: 棕石系 `floor_base=[115,108,95]` + wall_brick/wall_moss/floor_dirt/floor_b/floor_c/grid_line
- 火山 F6-10: 红岩系 `floor_base=[70,35,20]`
- 虚空 F11-15: 紫暗系 `floor_base=[45,32,55]`

**TilePalette 结构体 11 个字段**（`biome.h:8-11`）全部加载进运行时。

**但渲染时**（`game_map.cpp:270-295`）：
```cpp
Texture2D wall_data = rm.sprite_by_key("wall", wall_def);   // wall.png 存在 → 恒走纹理路径
...
SpriteRenderer::draw_sprite(wall_tex, wd, 0, {...},
    _dim({255,255,255,255}, bright));   // ← tint 恒为白色！palette 完全没参与
```

因为 `wall.png`/`floor.png` 存在，程序生成回退路径（唯一使用 palette 的路径，`game_map.cpp:272-275`）**永远不会执行**。结果：监狱/火山/虚空三层地图视觉完全相同。

### 根因 2: floor.png 是纯色

像素分析（PIL 实测）：
```
floor.png: 16x16, colors=1, unique_rows=1/16  → 100% 单色 #3B1D1B
wall.png:  16x16, colors=3, unique_rows=5/16  → 简单砖墙（可接受但平淡）
```

**floor.png 就是一块纯深棕色**。整个地图 80% 面积铺的是这个纯色块——这是"一直在同一个房间"感觉的最直接来源。

**讽刺的是**：程序生成回退（`gen_pixel_tile`, `sprite_renderer.cpp:70-78`）反而更好——基色+确定性噪点+石板接缝。当前架构下它永远不会被执行。

### 根因 3: 零 per-tile 变体

- `Tile` 结构体（`game_map.h:37-50`）无任何视觉变体字段
- 渲染循环（`game_map.cpp:365-373`）对每个 FLOOR tile 绘制**完全相同**的纹理+网格线
- 无 (x,y) 哈希、无随机变体选择、无装饰层
- 特殊房间地板有色块区分（`game_map.cpp:300-322`）——这是目前唯一的变体来源，但仅限 11 类特殊房

### 已有的可复用资产（盘点）

| 资源 | 状态 |
|------|------|
| `gen_pixel_tile` 程序生成（噪点+砖缝/石板缝） | ✅ 已实现，死代码 |
| TilePalette 11 色/群系（含 floor_dirt/floor_b/floor_c 变体色） | ✅ 已加载，死代码 |
| kenney_tiny_dungeon 132 张 tile | 仅 4 张门接线，128 张闲置（含地板/墙壁变体、装饰 tile） |
| `_add_noise` 确定性噪点函数 | ✅ 已实现（`sprite_renderer.cpp:40-57`） |

**结论：不需要任何新美术资产。缺的是"接线"。**

---

## 3. 方案设计（G10.4-B 实施范围）

### Fix 1: 群系 tint（~4 行，收益最大）

纹理绘制 tint 从白色改为 palette 色：
```cpp
// wall: _dim({255,255,255,255}, bright) → _dim(_palette.wall_face, bright)
// floor: 同理用 _palette.floor_base
```
- 立即实现 3 群系视觉分化（监狱棕 / 火山红 / 虚空紫）
- floor.png 纯色反而成为优势——它就是完美的 tint 底图
- wall.png 3 色结构 tint 后仍保留砖纹
- 无 palette 时维持白色 tint（向后兼容）

### Fix 2: per-tile 哈希变体（~20 行）

用 `(x*73856093 ^ y*19349663) % 100` 确定性哈希（同 seed 同图——确定性不破坏 replay）：
- **90%** tile: 正常 tint
- **6%** tile: `_palette.floor_dirt` tint（污渍变体）
- **4%** tile: `_palette.floor_b` tint（石块变体）

零新纹理、零新字段——纯渲染时计算，存档/replay/碰撞完全不受影响。

### Fix 3: wall 顶面高光（~6 行，可选）

WALL tile 且**下方是 FLOOR**时，底边 2px 画 `_palette.wall_top` 高亮线——模拟墙面受光，增加墙体立体感。纯 DrawRectangle 调用。

### 不做（明确排除）

- ❌ 不做几十张新 Tile（用户已明确反对）
- ❌ 不做房间类型主题色（特殊房已有，普通/战斗/精英房区分留 G10.5）
- ❌ 不动 Tile 结构体 / 存档格式 / world_validator
- ❌ 不接 kenney 地板变体（tint+噪点方案已够，留作后备）
- ❌ 不做光照/阴影方向（远期）

### 文件影响范围

| 文件 | 改动 | 行数 |
|------|------|------|
| `src/game/world/game_map.cpp` draw() | Fix 1 tint + Fix 2 哈希变体 + Fix 3 顶线 | ~30 行 |
| 测试 | 无新测试（纯视觉，无逻辑变化，replay 确定性由哈希函数保证） | 0 |

**总改动量：单文件 ~30 行渲染代码。符合"装饰不影响碰撞和地图逻辑"红线。**

---

## 4. Review Gate

等待批准：

- [ ] **D-A**: Fix 1 群系 tint（白→palette 色）——同意？
- [ ] **D-B**: Fix 2 per-tile 确定性哈希变体（6% 污渍 + 4% 石块）——比例/效果同意？
- [ ] **D-C**: Fix 3 wall 底边高光线——做不做？
- [ ] **D-D**: 本批完成后是否直接接 G10.4-A 攻击反馈强化（SWORD 命中节奏：hit_flash 时长/粒子数/三段差异/终结感）？

**预期效果**：监狱层暖棕地板+随机污渍、火山层红岩、虚空层暗紫，三群系一眼可辨，普通房间内部有细节变化——全部零新资产、~30 行代码。

批准后实施顺序：Fix 1 → Build/CTest → Fix 2 → Build/CTest → Fix 3（若批准）→ 桌面同步 → 实机截图 Review。
