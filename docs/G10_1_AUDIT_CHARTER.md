# G10.1 审计章程 — Asset Audit + Style Guide

> 状态: **待 Review Gate 批准** | 类型: 只读审计 | 基线: `b3143ec` (v1.3.1)
> 产出物: `docs/G10_ART_AUDIT.md` + `docs/STYLE_GUIDE.md` (骨架)
> 铁律: 本批次**不改任何源码、不替换任何美术、不动管线**。审计发现的问题只登记, 修复留给后续批次。

## 0. 目录实况校正 (侦察已核实, 审计以此为准)

| 目录 | 实际内容 |
|------|---------|
| `assets/sprites/` | 46 文件 — 项目自有精灵 |
| `assets/vendor/` | 330 文件 — 第三方包 (确认含 `kenney_tiny_dungeon`, 门贴图即取自此处) |
| `assets/` 汇总 | 368×PNG + 2×MP3 + 3×.tsx + 2×.tmx (Tiled 地图!) |
| `resources/` | **不是美术** — 23×JSON 数据 (enemies/bosses/items/biomes...) + `world/` 3 文件 |
| 音频主体 | 程序生成 (audio_server/wave_synth, 固定种 mt19937(42)), 文件音频仅 2×MP3 |
| 代码引用面 | ~54 处资源加载调用点 (load_texture/load_sound/play_sfx 等) |

用户原始设想 (resources/{textures,sprites,...}) 与实况不符 — 审计第一项输出即"权威目录地图"。

## 1. 资产清点 (①完整清点)

对 `assets/` 全量扫描, 产出一张主清单表, 每个文件一行:

- **引用状态**: 已使用 / 未使用 / 仅 vendor 内部互引
  - 方法: `git grep` 全量提取代码+JSON 中的路径字面量与拼接规则 → 构建引用图谱
  - 特别注意 ResourceManager 是否有运行时拼接路径 (前缀+名字), 静态扫描需覆盖
- **重复资源**: 同尺寸同内容 hash (MD5) 的重复文件; vendor 包与 sprites/ 的冗余拷贝
- **缺失资源**: 代码/JSON 引用了但磁盘不存在 (启动时静默失败的路径)
- **硬编码路径**: 路径字面量散落点统计 (已确认 door_renderer.cpp 等处), 按"应集中到 ResourceManager/清单"分级
- **缺失 fallback**: 加载失败的静默行为登记 (哪些绘制路径无占位图)
- **尺寸/像素密度**: 解析 PNG IHDR 逐文件记录 WxH; 标记 16/32/48/64 混用点
- **SpriteSheet 规格**: 逐 sheet 记录 tile 尺寸/行列数/命名规律 (kenney 是 16x16 网格, 与游戏 TILE_SIZE=32 的 2x 缩放关系要确证)
- **Tiled 资产 (.tmx/.tsx)**: 是否有加载代码? 无则登记为"未接线资产"

## 2. 视觉规范 (②Style Guide — 骨架, 答案由审计证据+你的决策填)

`docs/STYLE_GUIDE.md` 骨架章节, 每项附"现状证据 + 建议值 + 待决项":

- Tile Size / Character Base / Collision (已确认 32/32/28, 审计核实全链一致性)
- Pixel Scale: vendor 16px → 游戏 32px 的统一缩放策略
- Outline / Palette (从现有资产提取主色板) / Lighting
- UI Font (g_font/g_font_small 现状与码点覆盖)
- VFX Resolution (程序特效的锚点/分辨率基准)
- **核心决策题**: 最终视觉风格 — Kenney 统一? 像素手绘替换? 混合规则?
  (审计只给证据与选项, 结论由你拍板)

## 3. 方法与验证

- 全程只读; PNG 尺寸用 IHDR 头解析 (不依赖 raylib 窗口)
- 重复检测: MD5 分组 + 同名不同目录
- 引用图谱结果用抽样回查验证 (每类 ≥3 例人工核对)
- 审计文档所有结论附证据 (文件路径/行号/数字)

## 4. 明确不做

不换美术 / 不写加载代码 / 不建管线代码 / 不动 renderer / 不清 P3 / 不做性能优化

## 5. Review Gate

审计完成后停止, 等待批准; Style Guide 的"待决项"需你逐项拍板后才进入 G10.2 定稿。
