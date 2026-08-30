# STYLE GUIDE — 视觉规范 (G10.1 事实层骨架)

> 状态: 事实层已由 G10_ART_AUDIT.md 证据锁定; `【待决】` 项需 Review Gate 拍板后定稿。

## 已锁定事实 (代码/资产实测)

| 项 | 值 | 证据 |
|----|-----|------|
| Tile Size | **32×32** (TILE_SIZE) | config.h |
| 地图 | 40×30 格 (窗口 960×640 = 30×20 可视) | config.h |
| Character Base | 玩家 32×32 视觉盒 | player.cpp:44 `Entity(x,y,32,32,…)` |
| Collision | **玩家 28×28** (居中); 怪物 28×28 (视觉=碰撞) | player.cpp:44 / monster.cpp:145 |
| Source Art | **16×16 单帧** (46/46 自有精灵 + Kenney/Tiny Creatures) | sprites.json + IHDR 全量 |
| Pixel Scale | 16→32 **整数 2×**; 怪物 16→28 = **1.75× (违规点)** | game_renderer / entity 尺寸 |
| Outline | `#31263F` 暗紫描边 (自有精灵与 Kenney 共用) | 色板实测 |
| Palette | Kenney tiny 系: `#7C6052` `#B49B8B` `#DCCBC0` + 每精灵 3-7 色小色板 | 色板实测 |
| UI Font | 中文光栅文本 LoadFontEx(32/18), 预编译码位, BILINEAR; 链: assets→系统字体→Default | resource_manager.cpp:123 |
| Lighting | 无光照系统; 全屏 7% WorldReaction tint 为唯一氛围层 | game_scene.cpp:1901 |
| VFX | 程序生成 32×32 圆形贴图, DrawCircle 兜底, `fx_<rgb>` 缓存键 | game_renderer.cpp:98-108 |
| Audio | 程序合成 sfx×15 + BGM×4 (mt19937(42)); 外部 MP3×2 带 fallback | audio_server.cpp |
| Animation | **零动画帧** — 全项目静态单帧, sprites.json 结构已支持多帧扩展 | frame_rect 行主序 |

## Review Gate 裁决记录 (G10.1 → 已定稿)

| 决策 | 裁决 | 落点 |
|------|------|------|
| D1 核心风格 | **C: 混合规则化** — 地形=Kenney, 角色=自绘, VFX=程序; 共享色板约束 | 本规范红线 §定稿 |
| D2 怪物缩放 | 禁止非整数缩放为红线; 代码修正暂缓, 随 G10.5 角色美术批次落地 | 红线 4 |
| D3 LOCKED 门 | 必须视觉区分; 推荐方案: 门态专属贴图 ID + Overlay 色罩 | G10.3 门迁移批次 |
| D4 字体 | 采用可再分发的开源中文字体 (思源黑体/Noto Subset) 替换系统依赖 | G10.3 音频/字体清单批次 |
| D5 VFX | 程序 VFX 保留; 未来逐步像素化 (限定色板) | 远期, 不设期 |
| D6 tiny_creatures | 保留仓库, 状态 = Vendor Candidate (D1-A 备选素材库) | 管线清单标注 |
| D7 动画 | 渐进式增加; 优先战斗可读性 (受击/攻击先于待机) | sprites.json frame_count 已可承载 |

## 规范红线 (定稿后生效, 此为草案)
1. 新美术一律 16×16, 进入 `assets/sprites/`, **必须**同步登记 `resources/sprites.json`
2. 禁止新增硬编码资产路径 (现有 6 条为迁移债, G10.3 收编)
3. 色板须含 `#31263F` 描边与 Kenney 基色; 新色需入色板表
4. 实体渲染尺寸必须为 16 的整数倍缩放 (2×/1×)
5. 外部音频文件必须带 `FileExists`→程序合成 fallback
