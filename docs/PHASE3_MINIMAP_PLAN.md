# Phase 3 Minimap + Exploration Memory — 实施前审计与设计方案

**状态**: 待审核（未编码）
**前置**: Phase 2 Dungeon Topology 已收尾并提交（`a33bcdf`）

> 本报告遵循既定流程：**审计 → 设计 → 审核 → 编码 → 验证**。请审核通过后再进入编码，不要直接开写。

---

## 一、审计结论（现状事实）

### 1.1 渲染架构 —— 无 Camera2D，最小侵入友好

- 项目**没有使用 Raylib Camera2D 相机矩阵**。grep 全 `src/` 无 `BeginMode2D`/`GetWorldToScreen2D`。
- 世界空间靠 `GameScene` 的 `float _cam_x/_cam_y` 偏移 + **手写减法**渲染（`screen = world - cam`）。
- 唯一 `Camera2D` 出现在 `player.cpp:54` 的空桩 `Player::render(Camera2D&)`，从未被调用。
- **含义**：小地图不依赖相机管线，可完全在屏幕空间独立绘制；tile→缩略像素缩放需自算。

### 1.2 探索状态 —— 单数据源已就绪（满足核心要求）

- `Tile` 结构（`game_map.h:29-40`）:
  ```
  struct Tile { TileType type; bool is_walkable; bool is_visible; bool is_explored; };
  ```
- `GameMap::isVisible(x,y)` / `isExplored(x,y)`（`game_map.cpp:75-81`）是 const 查询。
- `GameMap::draw()` 已采用三层 FOV：`!is_explored`→不渲染；`is_visible`→全亮；否则→0.6 暗（`game_map.cpp:187-190`）。
- **小地图可直接读取 `isExplored/isVisible`，无需维护第二套探索状态**。这是最小侵入的核心前提。

### 1.3 探索状态数据流 & 当前限制

- **唯一写入方**：`update_fov(cx,cy,FOV_RADIUS)`（`game_map.cpp:98-121`），只在玩家跨 tile 时触发（`game_scene.cpp:956`）。
- **清空时机**：`enter_floor()` 调 `reset_visibility()`（`game_scene.cpp:320`）。
- **关键限制（已决策）**：save 系统只存 `dungeon_seed`（`save_manager.h:10-29`），**不存 tile 网格探索状态**。读档/下楼后地图重新生成、探索状态必然清空。
- **决策 A（确认）**：探索记忆 = **仅当层**。不引入跨层/持久化记忆，避免扩大 save 系统改动。

### 1.4 HUD/UI 架构

- 主 HUD 集中式：`GameRenderer::draw_hud(...)`（`game_renderer.cpp:656`），每帧在 `_render()` 调一次（`game_scene.cpp:1874`）。
- 覆盖层：同一 `_render()` 内按顺序叠放（背包 `:1912`、对话 `:2108`、Boss cinematic `:2121` 等）。
- 面板工具：`GameRenderer::draw_panel(Rectangle, title, bg)`（`game_renderer.cpp:29`）用 `DrawRectangleRounded`。
- **小地图可仿照 `draw_panel` + 屏幕坐标，作为 WX 覆盖层加在 `_render` 中**。

### 1.5 可复用标注数据源（全部可从 `GameScene`/`game_map` 读取）

| 地标 | 数据源 | 位置 |
|:---|:---|:---|
| 玩家 | `_last_player_tile_x/y` | `game_scene.h:281-282`（每帧已更新） |
| 楼梯 | `stairs_pos`（`{tx,ty}`） | `game_scene.h:113` |
| 特殊房间 | `game_map->special_rooms`（`cx,cy,type,triggered,discovered`） | `game_map.h:59` |
| Boss | `monsters` 中 `is_boss==true` 存活项 | `_get_boss()` `game_scene.cpp:1563` |
| 怪物 | `monsters`（遍历） | `game_scene.h:98` |
| 地面物品 | `ground_items`（`tile_x,tile_y`） | `game_scene.h:99` |

### 1.6 已知风险点

- **tile→缩略像素缩放**：需自算 `scale = minimap_size / (MAP_WIDTH*TILE_SIZE)`，无 `GetWorldToScreen2D` 可用。
- **实体位置换算**：怪物/玩家无 tile 字段，用 `game_map->pixel_to_tile(px,py)` 现算（`game_map.cpp:28-30`）。

---

## 二、设计决策（已确认）

| 决策点 | 选择 |
|:---|:---|
| **探索记忆范围** | 仅当层（不跨层、不持久化） |
| **小地图形态** | 右下角固定面板（约 160×120），快捷键 M 切换 |
| **实体标记策略** | 仅当前可见；Boss + 楼梯**常驻地标** |
| **未探索区表现** | **完全不显示**（画成面板背景色） |

---

## 三、设计方案

### 3.1 数据流（不引入第二套状态）

```
小地图每帧读取（只读）:
  game_map->isExplored(tx,ty)   → 是否绘制该 tile
  game_map->isVisible(tx,ty)    → 是否高亮（当前可见）
  game_map->tile_at(tx,ty)      → 颜色（Floor/Wall/Door/Stairs/Lava）
叠加标注（单独数据源）:
  _last_player_tile_x/y  → 玩家格
  stairs_pos             → 楼梯(常驻)
  _get_boss() tile      → Boss(常驻，若存活)
```

### 3.2 新增文件与职责（单一职责，组合优于继承）

| 文件 | 职责 | 说明 |
|:---|:---|:---|
| `src/game/ui/minimap.h/.cpp` | `MinimapRenderer` 类 | 封装小地图绘制逻辑，负责缩放 + 颜色 + 标注 |
| `tests/ui/minimap_test.cpp` | 单元测试 | 验证缩放映射、探索/可见颜色、标注、不泄露逻辑 |

`MinimapRenderer` 接口设计（仅公开必要方法）:
```cpp
class MinimapRenderer {
public:
    // 每帧绘制：传入当前地图+玩家+走廊+楼梯+Boss
    void draw(const GameMap* map, int player_tx, int player_ty,
              std::pair<int,int> stairs, std::vector<std::pair<int,int>> visible_monsters,
              const std::pair<int,int>* boss, Rectangle panel) const;
private:
    static Rectangle tile_rect(int tx, int ty, const Rectangle& panel, float scale);
    static Color tile_color(TileType t);
    void draw_marker(int tx, int ty, const Rectangle& panel, float scale, Color c) const;
};
```

### 3.3 最小侵入改动点

| 文件 | 改动 |
|:---|:---|
| `game_scene.h` | 加成员 `MinimapRenderer _minimap;`、`bool _show_minimap = false;` |
| `game_scene.cpp` | `_render()` 中在 HUD 后新增 `_draw_minimap()` 调用；`_process()` 中处理 M 键切换 |
| `game_renderer.h/.cpp` | 不动（小地图独立类，不塞进 GameRenderer，避免其职责膨胀） |

**明确不改**：`GameMap`（绝不为小地图加数据）、`GameRenderer::draw_hud`（保持集中式）、`save` 系统（探索记忆不持久化）、`update_fov`/`reset_visibility`（FOV 逻辑零改动）。

### 3.4 未探索完全不显示的实现

```
for tx,ty 全地图:
  if !game_map->isExplored(tx,ty): skip   // 不画任何像素 → 面板背景色
  else: draw tile_rect with tile_color(tile_at)  // 按 tile 类型上色
```

- 当前可见（`is_visible`）的 tile 用**亮色**；已探索但当前不可见的用**暗色**（仿主地图 0.6 亮度语义，但小地图用更简单两档）。

### 3.5 标注策略

- **玩家**：白色亮格（恒显）。
- **Boss**：红点（常驻地标，存活才画）。
- **楼梯**：黄色格（常驻地标）。
- **怪物/物品**：仅当其 `center tile is_visible` 才画（当前可见才显示，绝不泄露未探索）。
- **特殊房间**（可选，已探索才显）：若 `discovered` 画类型色点。

### 3.6 缩略图计算

```
MAP_WIDTH=40, MAP_HEIGHT=30, TILE_SIZE=32 → 世界像素 1280×960
panel 高约 120px → scale = 120/960 = 0.125
tile_rect(tx,ty) = {panel.x + tx*32*scale, panel.y + ty*32*scale, 32*scale, 32*scale}
               = {panel.x + tx*4,      panel.y + ty*4,      4, 4}
```
40×4 = 160px 宽 × 30×4 = 120px 高 —— 符合右下角面板尺寸。

---

## 四、自动化测试计划

### 4.1 MinimapRenderer 单元测试（`tests/ui/minimap_test.cpp`）

设计成不依赖 Raylib 窗口（分离"计算"与"绘制"），核心可测函数：
- `tile_rect(tx,ty,panel,scale)` → 返回正确屏幕矩形
- `tile_color(TileType)` → 类型→颜色映射
- 缩放系数：40×30 地图 → 面板应保持 4:3 宽高
- **不泄露逻辑**（纯函数）：给定探索掩码 + 实体 tile，只有 `isExplored` 的 tile 计入绘制集合；`is_visible` 才计入"高亮集合"

### 4.2 集成测试（可选，加到现有）

- 生成多 seed 地图 → 遍历所有 tile，确认 `isExplored` 掩码与 `is_visible` 分开
- Boss 存活时 `_get_boss()` 返回非空；Boss tile 恒被标注

### 4.3 验证清单（编码后）

- [ ] `MinimapRenderer` 单测全通过
- [ ] 38 现有测试无回归
- [ ] 实机：右下角小地图正常显示，M 键切换，未探索区域完全不显示
- [ ] 实机：走进已探索区域，地图痕迹"永久留下"（当层内）
- [ ] 实机：当前可见区域高亮
- [ ] 实机：room/corridor/door 在缩略图上有可辨识的色块差异
- [ ] 实机：走到走廊尽头前，看不到未探索怪物/Boss/物品

---

## 五、风险与边界

| 风险 | 对策 |
|:---|:---|
| 小地图 tile 仅 4px 可能过小难辨识 | 用实色块区分（墙灰/地板棕/门深棕/楼梯黄），不依赖纹理；可右键放大（二期可选项，本轮不做） |
| 实体密集时缩略图标点重叠 | 实体标记仅画点不画框；怪物重叠时可省略细节 |
| M 键与现有输入冲突 | 检查 `game_scene_input.cpp` 已用按键，M 若已被占用改用其它（如 `Tab` 或 `T`） |
| `_get_boss()` 返回裸指针 | 调用方必须判空后再用 `pixel_to_tile` |

---

## 六、审核要点（请确认）

1. **MinimapRenderer 独立类**方案是否认可？（避免塞进 GameRenderer）
2. **探索记忆仅当层** —— 读档/下楼后小地图区域清零，是否符合预期？（当前架构如此）
3. **Boss + 楼梯常驻地标** —— 即便未在当前视野，也显示楼梯方向，是否接受？（这是移动便利性 vs 信息的取舍）
4. **4px tile 粒度**（160×120 面板）是否够用，还是想要更小？
5. **M 键**切换是否合适？

请审核并反馈调整项，通过后我再进入 Phase 3 编码。
