# G10.3 — Visual Vertical Slice Plan (Design Gate)

> 状态: **待 Review — 未批准前不写任何代码**
> 日期: 2026-08-30
> 前置: `G10_ART_AUDIT.md` · `STYLE_GUIDE.md` · `G10_2_PIPELINE_DESIGN.md` (已完成)
> 本文档只回答: 选什么房间 / 复用什么 / 缺什么 / 改什么 / 不改什么

---

## 1. Slice 场景选择

**Q1: 普通地牢战斗房间 (Dungeon Combat Room)** — 已确认。

理由: 玩家 80%+ 时间在此场景; 覆盖 8 类视觉对象最完整; 不依赖任何特殊系统。

具体验证场景: F1「地牢入口」的一个含 2-3 只怪 + 1 个掉落物 + 1 扇 CLOSED 门的普通房间。

---

## 2. 核心事实: 盘点结果与预期不符

调研发现（详见 `G10_ART_AUDIT.md` + 本次逐行核实）:

> **普通战斗房间的 8 类视觉对象中, 7 类已经接入 16×16 像素精灵或已验证管线。**

| # | 对象 | 现状 | 结论 |
|---|------|------|------|
| 1 | 地板 Tile | `sprites.floor` → `floor.png` 已接线 (`game_map.cpp:270`) | ✅ 已有 |
| 2 | 墙壁 Tile | `sprites.wall` → `wall.png` 已接线 (`game_map.cpp:271`) | ✅ 已有 |
| 3 | 门 | Kenney tile_0003/0018/0022, Asset Manifest 已迁移 (G10.2-B2) | ✅ 已有 |
| 4 | 玩家 | `player_fire/ice/poison.png` 三元素已接线 (`player.cpp:291`) | ✅ 已有 |
| 5 | 怪物 | `mon_slime/orc/bomber/tank/charger/summoner.png` 已接线 (`monster.cpp:125`) | ⚠️ 部分缺 |
| 6 | 武器攻击效果 | **纯程序图元** (`game_renderer.cpp:70-155`) | ❌ 缺 |
| 7 | 掉落物 | `item_potion_red/blue` + 8 armor + charm 全接线 (`game_scene.cpp:2600`) | ✅ 已有 |
| 8 | HUD | **纯 DrawTextEx + 矩形, 零纹理** (`game_renderer.cpp:772-917`) | ❌ 缺 |

**修正后的 G10.3 目标**: 不是"从零接入美术", 而是 **修补 2 个真缺口 (VFX/HUD) + 补齐怪物专用图 + 统一风格细节**。

---

## 3. 现有资产复用表 (Q2)

### 保留不动

| 资产 | 用途 | 依据 |
|------|------|------|
| Kenney 门 4 tile | 门视觉 (Asset Manifest 管线已验证) | G10.2-B2 完成 |
| 自有 `sprites` 段 46 张 PNG | 实体/物品/房间图标全接线 | `G10_ART_AUDIT.md:10` |
| 思源黑体 + GuiFont::DrawTextCH | 中文 HUD 文本 | B3B 完成, 1831 码位 |

### 替换/补充来源

| 资产 | 来源 | 说明 |
|------|------|------|
| 怪物专用图 ×3 | **tiny_creatures** (180 张 0 接线) | 白色骨系 (tile_0050-0053/0151-0156) → skeleton; 紫系 (tile_0066-0067/0076) → shaman/dark_mage; 绿系 → slime 替换升级 |
| 攻击特效 ×1 | **程序生成像素纹理** | `gen_pixel_blast` (`sprite_renderer.cpp:232`) 已有, 沿用并调制 |
| HUD 基础 ×1 | **不引外部素材** | 第一批 HUD 只做布局优化, 不做纹理面板 (见 §6) |

### VFX 决策

程序 VFX 暂时保留 (STYLE_GUIDE D5 裁决), Slice 阶段不重写 VFX 系统, 只验证现有 `gen_pixel_blast` 程序纹理在攻击特效中的实际观感。

---

## 4. 资产缺口表 (Q3)

| 类别 | 当前 | Slice 需要 | 动作 | 工作量 |
|------|------|-----------|------|--------|
| 地板 | floor.png ✅ | 1 | 无 | 0 |
| 墙壁 | wall.png ✅ | 1 | 无 | 0 |
| 门 | Kenney 4 tile ✅ | 1 套 | 无 | 0 |
| 玩家 | 3 元素 ✅ | 1 | 无 | 0 |
| 怪物 | 6 种通用图 ⚠️ | 3 张专用 | tiny_creatures 提取: skeleton/shaman/slime | 小 |
| 武器攻击 | 程序图元 ❌ | 1 | 剑挥砍接入 `gen_pixel_blast` 像素爆点 | 小 |
| 掉落物 | 13 种 ✅ | 2 | 无 (已有红/蓝药水) | 0 |
| VFX | 程序圆形 ✅(暂定) | 1 | 验证观感, 不重写 | 0 |
| HUD | 零纹理 ❌ | 布局优化 | 金币图标 + 血条样式调整 | 小 |

**总改动量: 3 张怪物图 + 1 处 VFX 接入 + HUD 微调 — 远小于原估"8 类资产替换"。**

---

## 5. 实施顺序 (Q4)

### Step 1: 怪物专用图 (tiny_creatures → sprites)

1. 从 `assets/vendor/tiny_creatures/Tiles/` 复制 3 张到 `assets/sprites/`:
   - `tile_0151.png` (白骨) → `mon_skeleton.png`
   - `tile_0066.png` (紫系) → `mon_shaman.png`
   - `tile_0008.png` (绿系) → `mon_slime.png` (替换现有)
2. `resources/sprites.json` `sprites` 段登记新条目
3. `_monster_sprite_key()` (`monster.cpp:26-38`) 增加映射
4. `tools/extract_chars.py` 无需跑 (纯图片)

### Step 2: 武器攻击像素化验证

1. `_draw_slash_arc()` (`game_renderer.cpp:70-93`) 中 spark/flash 分支改用 `procedural_fx` 像素爆点纹理 (已有函数 `gen_pixel_blast`)
2. 只改 SWORD 普攻一条链, 其余武器后续批次

### Step 3: HUD 基础优化

1. 金币/钥匙显示加图标 (Kenney tile_0114 系宝石/钥匙图标)
2. 血条加内边框 + 受击闪烁样式
3. 不做纹理面板, 不动布局结构

### Step 4: 截图 Review Gate

1. 运行游戏进入 F1 普通房间
2. 截图: 完整房间视野 + 攻击瞬间 + 拾取瞬间
3. 与 Phase C 三个决策点 (字体/VFX/怪物风格) 一起交给用户 Review

---

## 6. 明确不做什么

- ❌ 不碰 Boss (F5/F10/F15 贴图维持现状)
- ❌ 不碰 Challenge Arena 视觉
- ❌ 不碰 Mirror Boss
- ❌ 不做大规模 UI 重做 (背包/圣物/商店面板不动)
- ❌ 不做 Cinematic/开场动画
- ❌ 不做全地图 tile 变体 (裂纹/青苔地板等程序化变体维持)
- ❌ 不重写 VFX 系统 (10 图元架构保留)
- ❌ 不引入新第三方素材库 (只用已有 kenney + tiny_creatures)
- ❌ 不改字体 (思源黑体冻结, Phase C 再决策)

---

## 7. 文件影响范围

| 文件 | 改动 | 类型 |
|------|------|------|
| `assets/sprites/` | +3 PNG (mon_skeleton/shaman + slime 替换) | 资产 |
| `resources/sprites.json` | sprites 段 +2 条目 (shaman), slime 条目指向新文件 | 配置 |
| `src/game/entities/monster.cpp` | `_monster_sprite_key()` +2 映射 (skeleton/shaman 名字规则) | 代码 (~10 行) |
| `src/game/systems/game_renderer.cpp` | `_draw_slash_arc()` spark 分支接 `procedural_fx` | 代码 (~8 行) |
| `src/game/systems/game_renderer.cpp` | `draw_hud()` 金币图标 + 血条样式 | 代码 (~30 行) |
| `tools/world_validator.py` | 无需改 (sprites 段校验已覆盖) | — |
| 测试 | 无新测试 (纯视觉, 无逻辑变化) | — |

**代码总增量: ~50 行, 资产增量: 3 张 PNG。符合"最小 Slice"原则。**

---

## 8. Review Gate

**等待用户批准以下决策点:**

- [ ] D-A: 怪物图来源确认 — tiny_creatures 白骨/紫系/绿系是否符合预期风格?
- [ ] D-B: VFX 处理 — SWORD 攻击特效接程序像素爆点, 观感是否可接受?
- [ ] D-C: HUD 范围 — 只做金币图标+血条样式, 不做面板纹理, 是否同意?
- [ ] D-D: 实施顺序 — 怪物图 → VFX → HUD → 截图, 是否同意?

**批准后进入实施, 每步完成后小批量验证 (build + ctest + 桌面同步)。**

---

## 附: 盘点数据来源

- 渲染管线逐行核实: `game_map.cpp` / `player.cpp` / `monster.cpp` / `game_scene.cpp` / `game_renderer.cpp` / `sprite_renderer.cpp`
- 资产清单: `resources/sprites.json` (sprites 47 条 + assets 6 条)
- vendor 库像素级分析: kenney 132 tile 分类 + tiny_creatures 180 tile 色系分类
- 前置文档: `G10_ART_AUDIT.md` · `STYLE_GUIDE.md` · `G10_2_PIPELINE_DESIGN.md`
