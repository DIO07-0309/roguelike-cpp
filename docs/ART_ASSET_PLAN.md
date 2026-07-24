# Roguelike Asset Acquisition Plan

> 从几何体渲染到像素艺术：全面美术资源规划
> 项目：C++ Raylib 5.0 Roguelike · 240+ 源文件 · 3 Biomes

---

## Part 1: 当前资源状态与需求

### 现状：100% 程序化几何体

| 元素 | 当前渲染方式 | 颜色来源 |
|:---|:---|:---|
| 地板 | `DrawRectangle` + 石板拼接线 | `tile_palette.floor_base/floor_joint` (Biome JSON) |
| 墙壁 | `DrawRectangle` 正面+顶面+砖缝+青苔 | `tile_palette.wall_face/wall_top/wall_brick` (Biome JSON) |
| 玩家 | 绿色圆角矩形 + 白色眼珠 | 硬编码 `(40,160,40)` |
| 怪物 | 彩色矩形 + 圆角 + 高光条 | 硬编码 per-monster |
| Boss | 彩色大矩形 + 红色光晕脉冲 | 硬编码 per-boss |
| 特效 | `DrawCircle` 脉冲/弧线/闪光 | VFX Recipes JSON |
| 粒子 | `DrawCircle` 半透明圆 | `biome.ambient` JSON |
| 字体 | `LoadFont("msyh.ttc")` | 系统字体 |
| UI | `DrawRectangle` + `DrawText` | 硬编码颜色常量 |

**结论：你的项目没有一行 `LoadTexture()`。这是 100% 的程序化渲染管道。**

### 优先级矩阵

| 优先级 | 类别 | 影响 | 工作量 |
|:---|:---|:---|:---|
| **P0** | Player sprite | 主角——必须立刻替换 | 低（1 个 spritesheet） |
| **P0** | Tile set (walls + floors) | 整个画面由它们组成——最大视觉变化 | 中（每个 Biome 1 个 tileset） |
| **P0** | Font (pixel-art font) | 所有文字——立即改变游戏感 | 低（1 个 .ttf） |
| **P1** | Enemy sprites | 每个 Biome 4 种敌人 | 中（12 张 sprite） |
| **P1** | Boss sprites | 3 个 Boss | 低（3 张大尺寸 sprite） |
| **P1** | VFX spritesheet | 攻击/技能特效 | 低（1 个 spritesheet） |
| **P2** | UI frame/panel | 背包/对话框/事件面板 | 低（9-patch 素材） |
| **P2** | Ambient particles | 灰尘/余烬/幽光 | 低（PNG 替代圆形） |
| **P2** | Sound effects | SFX 素材 | 低（.wav 替代程序化合成） |
| **P2** | Item/Relic icons | 装备/圣物图标 | 低（1 个 icon spritesheet） |

---

## Part 2: 三 Biome 素材清单

### Forgotten Prison（遗忘监牢·楼层 1-5）

| 类别 | 数量 | 说明 | 当前实现 |
|:---|:---|:---|:---|
| **Wall tiles** | 1 个 tileset (32×32) | 暗紫灰石砖墙 + 青苔变体 + 裂缝变体 | `palette.wall_face=(42,38,34)` → 替换为纹理 |
| **Floor tiles** | 1 个 tileset (32×32) | 石板地面 + 接缝 + 灰尘斑点 + 水渍 | `palette.floor_base=(115,108,95)` → 替换为纹理 |
| **Player** | 1 个 spritesheet | 4 方向 × 3 帧行走 + 1 帧攻击 + 1 帧待机 | 绿色圆角矩形 → 替换为精灵 |
| **Enemy: bone_soldier** | 1 个 sprite | 骷髅士兵——白骨 + 生锈铠甲 | `color=(180,160,140)` 矩形 |
| **Enemy: skeleton_archer** | 1 个 sprite | 骷髅弓箭手——持弓姿势 | `color=(170,150,130)` 矩形 |
| **Enemy: slime** | 1 个 sprite | 绿色史莱姆——弹跳动画 4 帧 | `color=(100,200,80)` 矩形 |
| **Enemy: shadow_stalker** | 1 个 sprite | 暗影潜行者——半透明+发光眼 | `color=(80,60,100)` 矩形 |
| **Boss: shadow_knight** | 1 个 sprite (48×48) | 暗影骑士——大铠甲 + 红眼 | 红色光晕矩形 |
| **Landmark: broken_cell** | 1 个 decoration | 牢房——断裂锁链、血迹、骷髅 | 图标 "⚒" |
| **Landmark: torture_chamber** | 1 个 decoration | 刑讯室——生锈刑具 | 图标 "†" |
| **Landmark: collapsed_tunnel** | 1 个 decoration | 坍塌隧道——落石堆 | 图标 "▼" |
| **Ambient particles** | 1 个 PNG | 尘埃粒子——灰紫色小点 | 半透明 `(140,135,150)` 圆形 |
| **VFX: melee_hit** | spritesheet 内 | 金色脉冲 + 火星飞溅 | VFX recipe |

### Ash Volcano（灰烬火山·楼层 6-10）

| 类别 | 数量 | 关键特征 |
|:---|:---|:---|
| Wall tiles | tileset | 暗红玄武岩 + 熔岩裂缝发光变体 |
| Floor tiles | tileset | 暗红暖色石板 + 灰烬斑点 |
| Player | (共用) | — |
| Enemy: fire_imp | 1 sprite | 火魔仆从——小体型+火焰光环 |
| Enemy: elite_orc | 1 sprite | 精英兽人——大斧+重甲 |
| Enemy: orc | 1 sprite | 兽人——中型+棍棒 (可复用监狱版) |
| Enemy: charger | 1 sprite | 冲锋兽人——冲刺姿势 |
| Boss: fire_demon | 1 sprite (48×48) | 地狱火魔——熔岩体型+火焰翅膀 |
| Landmark: lava_rift | 1 decoration | 熔岩裂缝——发光裂口 |
| Landmark: forge_ruins | 1 decoration | 废弃铁匠铺——铁砧半埋在岩浆中 |
| Landmark: fire_pillar | 1 decoration | 火柱——发光粒子 |
| Ambient particles | 1 PNG | 余烬粒子——橙红色 |

### Void Abyss（虚空深渊·楼层 11-15）

| 类别 | 数量 | 关键特征 |
|:---|:---|:---|
| Wall tiles | tileset | 深紫黑玄武岩 + 发光符文变体 |
| Floor tiles | tileset | 深紫黑地面 + 幽光裂缝 |
| Player | (共用) | — |
| Enemy: shadow_stalker | (复用) | — |
| Enemy: dark_mage | 1 sprite | 黑暗法师——斗篷+发光法杖 |
| Enemy: shadow_assassin | 1 sprite | 暗影刺客——苗条+双刃 |
| Enemy: void_walker | 1 sprite | 虚空行者——漂浮+触手 |
| Boss: demon_lord | 1 sprite (48×48) | 深渊之主·终焉——巨型+火焰光环 |
| Landmark: floating_altar | 1 decoration | 漂浮祭坛——浮石+发光 |
| Landmark: void_crack | 1 decoration | 虚空裂缝——扭曲空间裂口 |
| Landmark: ancient_gate | 1 decoration | 远古石门——发光符文 |
| Ambient particles | 1 PNG | 虚空粒子——深紫色幽光 |

### 总计（3 个 Biomes）

| 类别 | 数量 |
|:---|:---|
| Tilesets | 3（每个 Biome 1 套 wall+floor） |
| Player spritesheet | 1 |
| Enemy sprites | ~12 |
| Boss sprites | 3 |
| Landmark decorations | 9 |
| VFX spritesheet | 1 |
| UI 9-patch | 1 |
| Particle PNG | 3 |
| Pixel font | 1 |
| ~~Sound files~~ | (留到后续 G7 SFX 阶段) |
| **总计** | **~35 个素材文件** |

---

## Part 3: 搜索关键词（英文）

### Tilesets

```
pixel art dungeon tileset 32x32
pixel art prison wall tiles
pixel art volcanic cave tileset
pixel art abyss void tileset
pixel art dungeon floor stone tile
pixel art lava cave wall tiles
pixel art dark fantasy tileset
free roguelike tileset 32x32
```

### Player Character

```
pixel art knight sprite sheet 32x32
pixel art rogue adventurer spritesheet 4 direction
pixel art dungeon crawler character sprite
free pixel art hero spritesheet walking animation
top-down pixel art knight idle walk attack
darkest dungeon style character sprite
```

### Enemies（每种类型分开搜索）

```
pixel art skeleton warrior sprite
pixel art slime monster sprite sheet animated
pixel art shadow creature sprite dark
pixel art orc warrior sprite 32x32
pixel art fire imp demon sprite
pixel art dark mage wizard sprite
pixel art void monster tentacle sprite
```

### Boss

```
pixel art dark knight boss sprite large
pixel art fire demon boss sprite large
pixel art demon lord final boss sprite
dark souls style pixel art boss
```

### VFX

```
pixel art slash attack effect spritesheet
pixel art fire explosion effect sprite
pixel art ice shatter effect spritesheet
pixel art lightning bolt effect sprite
pixel art magic circle effect spritesheet
free pixel art vfx pack spritesheet
```

### UI

```
pixel art ui panel frame 9-patch
pixel art inventory ui panel
pixel art health bar ui frame
pixel art font ttf monospace
free pixel font ttf
```

### 推荐 itch.io 合集

```
pixel art dungeon pack
free roguelike asset pack
oatmeal pixel art
pixel fantasy rpg tileset
tiny dungeon tileset
```

---

## Part 4: 素材规格

### 统一规格

| 参数 | 值 | 原因 |
|:---|:---|:---|
| **Tile size** | 32×32 px | 匹配 `TILE_SIZE = 32` |
| **Player sprite** | 32×32 px | 1 tile 大——roguelike 标准尺寸 |
| **Enemy sprite** | 32×32 px | 标准敌人 |
| **Boss sprite** | 48×48 px | 比标准敌人大 1.5×，显著 |
| **Spritesheet 格式** | 单行或网格 PNG | Raylib `LoadTexture` → `DrawTextureRec` |

### Player Spritesheet 布局

```
单行 32×32 帧，5 列：
[Idle] [Walk1] [Walk2] [Walk3] [Attack]

共 4 行 = 4 个方向：
Row 0: 向下
Row 1: 向上
Row 2: 向左
Row 3: 向右

总尺寸: 160×128 px (5×32 宽, 4×32 高)
```

### Enemy Sprite

```
32×32 单帧 → 最简单
或 32×32 × 3 帧（Idle + Walk1 + Walk2）→ 有简单动画
```

### Tileset 布局

```
单张 PNG，网格排列：
例如 16×16 tiles → 512×512 px (16 cols × 16 rows)
或 8×8 tiles  → 256×256 px (8 cols × 8 rows)

每个 tile = 32×32 px
至少需要：
- 地板 × 4 (普通 + 裂缝 + 暗色 + 高光)
- 墙壁 × 4 (正面 + 顶面 + 裂缝 + 暗色)
- 装饰 × 2 (青苔/符文/血渍)
- 楼梯 × 1
```

### VFX Spritesheet

```
网格布局，每个效果占据 32×32 或 64×64 的 cell
至少包含：
- 斩击弧线 × 1 frame
- 脉冲圆 × 3 frames (小/中/大)
- 闪光 × 1 frame (白色全屏)
- 爆炸/火花 × 4 frames
```

### 粒子

```
16×16 或 8×8 的单个 PNG，带 alpha 通道
biome.ambient.count 控制每帧生成数量
不需要 spritesheet——单个小PNG即可
```

---

## Part 5: 架构接入分析

### 不需要新增系统

| 现有架构 | 接入方式 | 改动 |
|:---|:---|:---|
| `ResourceManager` (`resource_manager.h/cpp`) | `LoadTexture()` + `UnloadTexture()` | +15 行 |
| `tile_palette` in `biomes.json` | 新增 `"tileset"` 字段 → 纹理路径 | +1 行/JSON |
| `VFX Recipes` in `vfx_recipes.json` | 新增 `"texture"` 字段 → spritesheet 路径 | +1 行/recipe |
| `PresentEvent` in `presentation_system_director` | 已有 `dispatch()` 管道——直接消耗 | 0 行 |
| `FloorConfig` in `floor_config.cpp` | 已有所有数据——不碰 | 0 行 |

### 需要新增的最小模块

| 模块 | 说明 | 文件 |
|:---|:---|:---|
| **`SpriteDef`** | 在 `resources/sprites.json` 中定义精灵——ID、spritesheet 路径、帧数、帧大小 | +1 JSON + 1 头文件 |
| **`AnimationDef`** | 在 `resources/animations.json` 中定义动画——帧序列、速度、循环 | +1 JSON + 1 头文件 |
| **`SpriteRenderer`** | `draw_sprite(rect, sprite_def, frame, direction)` → `DrawTextureRec` | +1 头文件 |
| **`TileRenderer` 升级** | 将 `wall_face`/`floor_base` 的颜色检查替换为 `DrawTextureRec`（若 tileset 可用） | ~20 行修改 |

### 总架构影响

```
现有架构（不碰）:
├── Player/Monster/Boss 类 → 属性不变
├── CombatSystem → 0 改动
├── Biome/FloorConfig JSON → 只新增字段
├── VFX pipeline → 只新增纹理
└── EventBus/Registry → 不变

新增（在现有框架内）:
├── resources/sprites.json
├── resources/animations.json
├── src/game/resources/sprite_def.h
├── src/game/rendering/sprite_renderer.h
└── assets/*.png (素材目录)
```

**投资回报**：10 行代码改动 + 新资产管理器 → 整个游戏从几何渲染升级到像素艺术。

---

## Part 6: Vertical Slice（遗忘监牢 Demo）

### 最低可行素材清单

| 类别 | 素材 | 数量 | 搜索关键词 |
|:---|:---|:---|:---|
| Player | Knight/Rogue spritesheet (4-dir×3 walk + idle) | 1 | `pixel art knight sprite sheet 32x32 4 direction` |
| Wall tiles | Prison stone wall tileset (32×32) | 1 set | `pixel art dungeon stone wall tile 32x32` |
| Floor tiles | Stone floor tileset (32×32) | 1 set | `pixel art stone floor dungeon tile 32x32` |
| Enemy | Skeleton warrior sprite (32×32) | 1 | `pixel art skeleton warrior sprite 32x32` |
| Enemy | Green slime sprite (32×32) | 1 | `pixel art slime monster sprite sheet` |
| Boss | Dark knight boss (48×48) | 1 | `pixel art dark knight boss 48x48` |
| VFX spritesheet | Slash/pulse/flash | 1 sheet | `pixel art vfx pack slash pulse` |
| Pixel font | .ttf monospace (中文兼容或英文) | 1 | `pixel font ttf free monospace` |
| UI panel | 9-patch dark frame | 1 | `pixel art ui panel frame 9-patch` |
| **总计** | | **9 个素材** |

### 这些素材让你可以跑一个怎样的 Demo

- 玩家是像素骑士，四方向行走+攻击动画
- 墙壁/地板是石板纹理（掉程序化几何）
- 史莱姆+骷髅兵有像素精灵
- 暗影骑士 Boss 帧 48×48 有大红眼特效
- 攻击有斩击弧线特效
- UI 有像素边框
- 像素字体贯穿全界面

### 推荐来源优先级

| 来源 | 为什么 |
|:---|:---|
| itch.io → "pixel art dungeon pack" | 一站式 tileset + 角色 + 敌人 |
| Kenney.nl → "Tiny Dungeon" / "Pixel Platformer" | 免费 + CC0 许可 + 32×32 完美匹配 |
| OpenGameArt → "dungeon crawl" | 按需搜索单个素材 |
| Unity Asset Store | 高质量但需要检查许可 |
| GameDev Market | 专业但付费 |

### 最推荐的单次购买

**Kenney "Tiny Dungeon"**（itch.io 或 kenney.nl）：
- Tileset 含 walls/floors/stairs/decoration
- 角色 spritesheet 4-dir walk
- 敌人 sprites（slime, skeleton, bat, orc）
- Boss 尺寸的角色
- 像素字体
- UI elements
- **全部 CC0 许可**（可商用）
- **全部 32×32 完美匹配你的 TILE_SIZE**

一个包覆盖你的 P0 和大部分 P1 需求。
