# Roguelike Visual Style Guide & Asset Strategy

> 目标：用最少素材，获得最大的视觉完成度跃升
> 引擎：Raylib 5.0 · TILE_SIZE=32 · 当前全几何渲染

---

## 1. 视觉风格建议：Dark Pixel Fantasy

不要试图做 16-bit JRPG 风格或高清 2D。

你的游戏 DNA 决定了唯一正确的方向：

**Dark Pixel Fantasy** — 暗色调像素艺术，类似《Darkest Dungeon》的阴暗感 +《Enter the Gungeon》的像素精度 +《Slay the Spire》的 UI 克制力。

### 为什么这个方向是正确的

| 你的项目特征 | 匹配的风格特征 |
|:---|:---|
| 三个 Biome 都阴暗（监狱/火山/深渊） | 低饱和度调色板，环境光 < 30% |
| 31 种敌人，6 个 Boss | 每个单独精灵成本高 → 统一设计语言 + 调色板交换复用 |
| VFX 系统已完整 | 小尺寸 spritesheet 即可大幅提升攻击反馈 |
| UI 全几何体绘制 | 像素 UI 框架 + 像素字体 = 成本极低 |
| 类 Roguelike 永久死亡 | 黑暗主题天然匹配 |

### 不要做的风格

| 风格 | 原因 |
|:---|:---|
| 16-bit JRPG | 太亮，与你 Biome 的暗色调矛盾 |
| HD 手绘 2D | 精灵数 ×3-5 成本，且与像素 VFX 不协调 |
| Low-poly 3D | 整个渲染管道需要重写 |
| 纯 ASCII | 已有完整系统，降级回去浪费 |

### 核心调色板约束

| 颜色域 | 值 | 用途 |
|:---|:---|:---|
| 黑色 | #1a1a2e | 地板底色 |
| 暗灰 | #3d3d5c | 墙壁正面 |
| 暖灰 | #6b6b7b | 墙壁高光 |
| 血色 | #8b1a1a | 血渍/危险提示 |
| 金色 | #d4a017 | 稀有物品/VFX 强调 |
| 幽灵蓝 | #4a4ae8 | 魔法特效 |
| 毒绿 | #2e8b2e | Buff/毒特效 |

每个 Biome 在此基础调色板上叠加自己的色调偏转（Prison +紫灰，Volcano +暗红，Abyss +深紫黑）——这与你的 `biome.json` 中已有的 `tile_palette` 字段完美对齐。

---

## 2. 必须寻找的资产分类（按替换优先级）

### P0 — 替换后有"即刻 wow 感"

| 优先级 | 资产 | 数量 | 当前是 | 替换效果 |
|:---|:---|:---|:---|:---|
| **#1** | Tileset（墙+地板）1 套 | 1 | 单色矩形 | 整个画面从"几何测试"变"游戏" |
| **#2** | 像素字体 .ttf | 1 | 微软雅黑系统字体 | 所有文字立即像素化 |
| **#3** | 玩家 spritesheet | 1 | 绿色圆角矩形+眼珠 | 主角辨识度从 0→100 |
| **#4** | 史莱姆 sprite | 1 | 绿色矩形 | 第一个敌人有形象 |

**仅这 4 个资产就能让你的 Demo 和现在的版本看起来像是两个不同的游戏。**

### P1 — 替换后完成视觉闭环

| 优先级 | 资产 | 数量 |
|:---|:---|:---|
| **#5** | 骷髅/亡灵敌人 sprite (3 种) | 3 |
| **#6** | Boss sprites (3 种) | 3 |
| **#7** | VFX spritesheet（斩击/脉冲/爆炸） | 1 |
| **#8** | UI 面板 9-patch | 1 |
| **#9** | 地面物品/圣物 icon spritesheet | 1 |

### P2 — 锦上添花

| 优先级 | 资产 |
|:---|:---|
| **#10** | 余下 8 个 Biome 敌人 sprite |
| **#11** | Landmark 装饰 (per Biome) |
| **#12** | 粒子 PNG (灰尘/余烬/幽光) |
| **#13** | 音效 .wav 包 |

---

## 3. 每类资产搜索关键词（英文）

### Tileset（最优先）

```
pixel art dungeon tileset 32x32 dark
pixel art stone wall floor tile set 32x32
free roguelike tileset 32x32 dark fantasy
pixel art prison dungeon tilemap
Tiny Dungeon tileset kenney
oatmeal pixel art dungeon
```

**搜索策略**：先搜 "tileset pack"（一套含墙+地板+装饰），不要分开买单个 tile。

### Player character（最优先）

```
pixel art knight sprite sheet 32x32 4 direction
pixel art rogue hero spritesheet top-down walk idle attack
pixel art adventurer character sprite sheet 32x32
free pixel art hero sprite 4 direction walk
darkest dungeon style pixel character 32x32
```

**关键筛选条件**：必须有 4 方向（上下左右），否则移动动画对不上。不需要超过 4 帧行走。

### 像素字体（最优先）

```
pixel font ttf monospace free
retro pixel game font ttf
8-bit pixel font ttf
free pixel font for game dev
m5x7 pixel font
```

**关键**：必须是 .ttf 格式（Raylib `LoadFontEx` 支持），不能是 .png bitmap font。

### 敌人 sprite

```
pixel art slime monster sprite sheet 32x32 animated
pixel art skeleton warrior sprite 32x32
pixel art skeleton archer sprite 32x32
pixel art shadow creature sprite dark 32x32
pixel art orc warrior sprite 32x32
pixel art fire demon imp sprite 32x32
pixel art dark mage wizard sprite 32x32
pixel art void monster tentacle sprite 32x32
```

### Boss sprite

```
pixel art dark knight boss sprite 48x48
pixel art fire demon boss sprite large
pixel art demon lord final boss sprite 48x48
pixel art boss monster spritesheet large
```

### VFX spritesheet

```
pixel art vfx pack slash explosion spritesheet
pixel art magic effects spritesheet free
pixel art sword slash effect sprite
pixel art fire explosion spritesheet
free pixel art vfx spritesheet pack
```

### UI

```
pixel art ui panel frame 9-patch
pixel art inventory window frame
pixel art health bar ui frame
pixel art button ui game
```

### 推荐单个搜索词覆盖最多的包

```
pixel art dungeon pack free
kenney tiny dungeon
oatmeal pixel art rpg pack
free roguelike asset pack itch.io
pixel art dark fantasy complete pack
```

---

## 4. 推荐尺寸规格

| 资产 | 尺寸 | 说明 |
|:---|:---|:---|
| Tile | 32×32 px | 匹配 `TILE_SIZE=32`，无需改代码 |
| Player | 32×32 px | 1 tile 大小——roguelike 标准 |
| Enemy (普通) | 32×32 px | 与玩家同级 |
| Enemy (精英/Boss) | 48×48 px | 1.5× 区分度明显 |
| Player spritesheet | 4 行 × 3-5 列 | 4 方向 × (idle + walk2 + attack) |
| Enemy spritesheet | 1 行 × 3 帧 | idle + walk1 + walk2 就够了 |
| VFX spritesheet | 64×64 per cell | 3×3 或 4×4 网格 |
| Pixel font | 任意 .ttf | 8-16px 字号范围 |
| UI panel | 16×16 tileable | 便于 9-patch 拉伸 |
| 粒子 | 8×8 或 16×16 | 单个 PNG，不需要 spritesheet |

### 为什么不需要更多帧动画

你的战斗是**即时制不是回合制**——攻击间隔 0.5 秒。3 帧行走 + 1 帧攻击足够。超过 4 帧的动画在 Roguelike 中 ROI 极低，因为玩家注意力在地图和敌人位置上，不在角色的流畅动画上。

---

## 5. 哪些资产应该优先替换

```
替换顺序（ROI 降序）:

1. ══════ WALL + FLOOR TILESET ══════
   理由: 屏幕 80% 是墙和地板
   效果: 即刻改变整个游戏的视觉感知
   成本: 1 次搜索 → 1 个 32×32 tileset PNG

2. ══════ PIXEL FONT .TTF ══════
   理由: 所有文字——标题/技能/HUD/对话/菜单
   效果: UI 从"系统字体"变为"游戏字体"
   成本: 1 个 .ttf 文件

3. ══════ PLAYER SPRITESHEET ══════
   理由: 玩家是最容易识别的元素
   效果: 主角有形象了
   成本: 1 个 spritesheet PNG

4. ══════ ENEMY SPRITES ══════
   理由: 战斗体验的核心
   效果: 敌人不再是彩色方块
   成本: 每个类型 1 个 sprite

5. ══════ VFX SPRITESHEET ══════
   理由: 攻击反馈——"打到东西了"
   效果: 提升战斗打击感
   成本: 1 个 spritesheet

6. ══════ BOSS SPRITES ══════
   理由: Boss 是游戏高潮
   效果: 终极敌人有压迫感
   成本: 每个 Boss 1 个大 sprite

7. ══════ UI FRAME ══════
   理由: 背包/对话/事件面板的容器
   效果: UI 从原始矩形变为风格化面板
   成本: 1 个 9-patch asset
```

---

## 6. 哪些可以以后程序生成（不需要找素材）

| 元素 | 保持程序化 | 原因 |
|:---|:---|:---|
| HP 条 / XP 条 | ✅ | 纯色填充即可，像素风边框用 `DrawRectangleLinesEx` |
| 地板颜色变化 | ✅ | 已有 `tile_palette` + seed-driven 噪声——买了 tileset 后可以从 tileset 采样颜色变体 |
| 楼梯光柱脉冲 | ✅ | 现有的正弦波脉冲+向下箭头已经很好——替换为 spritesheet animation 需要额外帧数 |
| 伤害数字浮动 | ✅ | `DrawTextEx` + `dmg_color_for()` 已经完成了 3-tier 着色 + alpha 淡出 |
| 屏幕震动 | ✅ | Camera shake 已有，看不出来"程序化" |
| 粒子（灰尘/余烬） | ✅ | 替换为 8×8 PNG 半透明圆即可，不需要搜索，用任意工具画 1 像素 |
| 特殊房间图标 | ✅ | +/$/~ 符号在 tileset 地板色背景下已经足够清晰 |
| 时停 B&W 叠加层 | ✅ | 现有的半透明灰色矩形 + 脉冲文字已经达到效果 |

---

## 7. Vertical Slice 推荐方案

### 只需要找 6 个素材

| 素材 | 搜索词 | 来源建议 |
|:---|:---|:---|
| **Tileset**（墙+地板+楼梯） | `dark dungeon tileset 32x32 pixel art free` | Kenney "Tiny Dungeon" 或 itch.io |
| **Player spritesheet** | `knight hero pixel art spritesheet 32x32 4dir` | Kenney "Tiny Dungeon" 自带角色 |
| **Slime sprite** | `pixel art slime sprite sheet 32x32 animated` | Kenney 包 | 
| **Skeleton sprite** | `pixel art skeleton warrior sprite 32x32` | itch.io → "pixel art skeleton" |
| **Pixel font** | `pixel font ttf monospace free` | itch.io → "m5x7" 或 "PixelFont" |
| **VFX spritesheet** | `free pixel art vfx pack spritesheet` | itch.io → "VFX pack" |

Kenney "**Tiny Dungeon**" 包**单次下载覆盖 #1 #2 #3 #4**四项。你再补 #5 和 #6 就行了。

### 接入工作量

| 工作 | 改动 |
|:---|:---|
| Tileset → `GameMap::draw()` | `DrawRectangle(...)` → `DrawTextureRec(tileset_tex, src_rect, dst_rect, WHITE)` — ~20 行 |
| Player spritesheet → `Player::draw_no_cam()` | 同上，根据 `direction` 选择行，根据 `_frame` 选择列 — ~25 行 |
| Enemy sprite → `Monster::draw()` | 同上，根据 `monster_type` → `visual_id` → sprite — ~15 行 |
| Font → `ResourceManager` | 加一个 assets/ 路径到字体候选列表 — ~3 行 |
| VFX spritesheet → `VFXServer` | 把 `DrawRing`/`DrawCircle` 替换为 spritesheet frame — ~25 行 |

**总计：~90 行代码改动，0 行 Gameplay 改动。**
