# G10 Art Audit — 资产与视觉规范审计 (G10.1)

> 只读审计 | 基线 `b3143ec` (v1.3.1) | 配套: `STYLE_GUIDE.md` (事实层) / `G10_1_AUDIT_CHARTER.md` (章程)
> 方法: 全量文件扫描 + PNG IHDR 尺寸解析 + MD5 去重 + 引用图谱 (src+resources 全字面量) + WPF 解码色板抽样

## 1. 权威目录地图

| 路径 | 内容 | 文件数 | 引用状态 |
|------|------|-------:|---------|
| `assets/sprites/` | 项目自有 16×16 精灵 (玩家/怪/BOSS/NPC/物品/房间) | 46 PNG | **46/46 全部接线** |
| `assets/vendor/kenney_tiny_dungeon/` | Kenney CC0 包 (Tiles 132 + Tilemap 2 + Examples + Tiled + License) | 137 | **仅 4 张门贴图接线** |
| `assets/vendor/tiny_creatures/` | Kenney CC0 包 (Tiles + Tilemap + Examples + Tiled + License) | 193 | **0 接线 (整包闲置)** |
| `assets/*.mp3` | timestop / domain_expand 外部音效 | 2 | 2/2 接线 |
| `assets/*.ttf/.ttc` | 中文字体 (simhei/msyh) | 0 (引用了但不存在) | **缺失** |
| `assets/vendor/*/[Tiled]/` | Tiled 编辑器工程 (.tmx×2 .tsx×3) | 5 | **0 接线** |
| `resources/` | **数据 JSON (23), 非美术** + `resources/sprites.json` (精灵清单) | — | 管线核心 |

结论: 仓库只有**两个**资源根 — `assets/`(美术/音频) + `resources/`(数据)。原设想的 `resources/textures|audio` 不存在。

## 2. 引用图谱 (谁在真正运行)

53 个去重路径字面量, 出处:

| 出处 | 数量 | 方式 |
|------|-----:|------|
| `resources/sprites.json` | 47 | **精灵清单** (key → file + frame_w/h) — 事实上的资源管线中心 |
| `src/game/rendering/door_renderer.cpp` | 4 | 硬编码 Kenney 门贴图 |
| `src/game/audio/audio_server.cpp` | 2 | 硬编码 MP3 (有 FileExists→程序合成 fallback ✓) |
| `src/game/resources/resource_manager.cpp` | 2 | 字体候选链 (缺失有系统字体 fallback) |

**真正运行中的资产 (53)**:
- 玩家×3 (fire/ice/poison)、怪物×6、BOSS×2、NPC×4、武器×5、防具×7、药水×2、饰品×1
- 房间×8、墙/地×2、宝箱/泉水×3
- Kenney 门贴图×4 (OPEN=0003, CLOSED=0022, **LOCKED=0022 (与 CLOSED 同图!)**, SEALED=0018)
- 音频×2 (jojo_timestop.mp3, domain_expand.mp3) + 程序合成 sfx×15 + 合成 BGM×4
- 字体×0 (两个本地字体缺失, 走系统回退)

## 3. 未接线资产

| 组 | 数量 | 说明 |
|----|-----:|------|
| `vendor/tiny_creatures/` 整包 | 193 | 0 引用。含 Tiles/Tilemap/Examples/Tiled |
| `vendor/kenney_tiny_dungeon/` 其余 | 133 | 137 中仅 4 接线。Tiles/ 目录 132 张全部闲置 |
| Tiled 工程 (.tmx/.tsx) | 5 | 0 引用 (代码中 "tiled" 命中均为 `ProjectileDef` 假阳性) |
| 大图 (非 16×16) | 10 | 4×800x450 + 640x800 + 2×203x186 + 192x176 + 160x288 + 170x306 — 全部为包内 Preview/Tilemap, 0 引用 |
| License/Tilesheet txt | 3 | CC0 许可文件 (保留) |

闲置率: 378 文件中 325 未接线 (86%) — 全部集中在 vendor 包, 无自有资产浪费。

## 4. 缺失资源 (引用→磁盘不存在)

| 路径 | 引用处 | 后果 |
|------|--------|------|
| `assets/simhei.ttf` | resource_manager.cpp 字体链 | 跳过该候选, 落到 `C:/Windows/Fonts/simhei.ttf` |
| `assets/msyh.ttc` | 同上 | 同上 |

**风险**: 中文 UI 依赖系统字体存在; 缺 simhei/msyh/simsun 的机器 → `GetFontDefault()` → **英文界面**。且 msyh/simhei 是微软授权字体, 不应打包进仓库 (当前"缺失"反而规避了授权问题) — G10.2 需决策: 换开源中文字体 (思源/Noto) 或接受系统依赖。

## 5. 重复资源 (MD5)

仅 3 组 6 文件, 全部 vendor 内部: kenney `tile_0070==tile_0094`、`tile_0081==tile_0083` (包内别名); `kenney/Tilemap/tilemap.png == tiny_creatures/Tilemap/Kenney_tiny_dungeon.png` (跨包重复, tiny_creatures 内嵌了 kenney 图集)。**自有 sprites 零重复**。无需清理, 登记即可。

## 6. 硬编码路径与管线风险

| 等级 | 点 | 说明 |
|------|-----|------|
| 低 | door_renderer.cpp 4 条 | 唯一绕过 sprites.json 的美术路径; 且 LOCKED 复用 CLOSED 贴图 → 玩家无法目视区分锁门/关门 (G10.5 待决) |
| 低 | audio_server.cpp 2 条 | 有 FileExists→合成 fallback, 模式良好 |
| 无 | sprites.json 47 条 | 清单已中心化 — **G10.2 管线应以此文件为基准扩展**, 不是从零建 |
| 中 | 字体链含 5 个系统路径 | 可移植性风险 (见 §4) |

## 7. Fallback 体系 (现状)

1. **纹理**: `ResourceManager::load_texture` — 文件缺失→LOG_WARN+id=0; 实体/地图侧缺失→`procedural_sprite/procedural_tile` 程序生成 16×16 (monster.cpp:82, player.cpp:151, game_map.cpp:273) — **两级回退完备**
2. **字体**: 6 级链 (assets×2 → Windows 系统字体×4) → `GetFontDefault` (英文界面)
3. **音效**: 外部 MP3 → `FileExists` 判定 → 程序合成 (`_compile_*`, mt19937(42) 固定种)
4. **VFX**: `procedural_fx` 32×32 贴图 → `DrawCircle` 纯矢量兜底

## 8. 尺寸与像素密度

- **358/368 PNG 均为 16×16** (含全部 46 张自有精灵) — 资产底子高度统一
- 10 张例外全是包内 Preview/Tilemap 大图 (未接线)
- 渲染链: 16px 美术 → `TILE_SIZE=32` 格子 (**整数 2×**), 窗口 960×640 (30×20 格), 小地图 4px/格
- **像素密度不一致点 (唯一实锤)**: 怪物 `Entity(x,y,28,28)` — 视觉=碰撞=28, 16px 美术→28px = **1.75× 非整数缩放** → 像素宽窄不一。玩家为 32 视觉/28 碰撞 (整数缩放, 正确范式)
- 字体: `LoadFontEx(32/18)` + 预编译中文码位 + BILINEAR; 纹理未显式设 filter (raylib 默认 point → 像素风成立, 但 G10.2 应显式化)

## 9. SpriteSheet 规格

- `sprites.json` 每项声明 `frame_w/frame_h=16`; `SpriteRenderer::frame_rect` 按行主序取帧 — 单帧文件 frame_count=1, 结构可扩展多帧动画 **但当前全项目零动画帧** (全部静态单帧)
- vendor 图集: kenney `Tiles/tile_NNNN.png` 单帧散图 132 张 + `tilemap_packed.png` (192×176 = 12×11 格); tiny_creatures 同构
- Tiled 工程未接线 — 若未来做地图编辑需自建 tmx 加载器或放弃 Tiled 路线 (G10.3 决策)

## 10. 视觉风格事实 (实测)

- **色板**: 自有精灵与 Kenney 共用色系 — 描边 `#31263F` (暗紫夜色), 皮肤/地面 `#B49B8B`/`#7C6052`/`#DCCBC0`, 单精灵色数 3-7 → **当前已是"Kenney 调色板系 16px 像素风"**, 不是风格混搭
- kenney tilemap_packed 实测 771 色 (含过渡色); 自有精灵严格小色板
- VFX: 程序生成圆形贴图 + DrawCircle, 与像素风存在质感断层 (审计 §STYLE 待决)
- UI: 即时光栅中文文本 (raylib DrawTextEx), 无九宫格/贴图 UI
- WorldReaction 全屏 7% tint 是唯一"光照"; 无真光照系统
