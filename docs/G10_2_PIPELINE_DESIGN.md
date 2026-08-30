# G10.2 — Asset Pipeline Design (设计稿, 待 Review Gate)

> 状态: **设计, 不写代码** | 基线 `b3143ec` + G10.1 审计 + D1-D7 裁决
> 核心原则 (用户指令): **不重新发明 AssetManager** — `resources/sprites.json` 已是事实管线中心, 本设计只做规范化与补边。

## 1. 现状数据流 (审计证据)

```
[已存在] assets/sprites/*.png  (Asset File)
             ↑ path
resources/sprites.json "sprites": { "mon_slime": {file, frame_w, frame_h, frame_count} }   (Manifest)
             ↓ load_sprite_config()
ResourceManager._sprite_defs[key] ──sprite_by_key(key)──→ _texture_cache[path]   (ResourceManager)
             ↓ Asset ID = json key ("mon_slime")
game_renderer / entity 代码 → SpriteRenderer::draw_sprite → Renderer
```

**结论: 四层链路已通, 缺的是** — ① 门/音频/字体三类资产未进清单 ② 无 ID 命名域规范 ③ 无 Render Rule/Fallback 声明位 ④ 无清单完整性校验测试 ⑤ 6 条硬编码路径旁路。

## 2. 概念定义 (六要素, 全部落到清单字段)

| 概念 | 定义 | 清单字段 |
|------|------|---------|
| **Asset Source** | 资产出处 (vendor 包名 / 自绘 / 程序生成) | `"source"`: `kenney_tiny_dungeon` \| `self` \| `procedural` |
| **Asset ID** | 全局唯一逻辑名, `<域>.<名>`, 业务代码只允许出现 ID | manifest 的 key (迁移后) |
| **Asset Path** | 磁盘相对路径, **只允许出现在 manifest** | `"file"` |
| **Render Rule** | 渲染方式声明 (帧规格/缩放基数/filter/overlay) | `"render"`: {frame_w, frame_h, frame_count, scale_base, filter, overlay} |
| **Pixel Scale** | 16px 美术 → 逻辑格的整数倍缩放 (红线: 仅 1×/2×) | `scale_base` (=16, 渲染端 ×TILE_SIZE/scale_base) |
| **Fallback Policy** | 缺失时的降级序列 | `"fallback"`: 见 §5 矩阵 |

## 3. 目标链路 (G10.3+ 实施后的样子)

```
Asset File (assets/**)
    ↓ 登记 (人工/脚本, PR 审查)
Asset Manifest (resources/sprites.json → 更名 assets.json, schema v2)
    ↓ load_assets()  — 校验后一次装载
ResourceManager  — tex("door.closed") / sfx("timestop") / font("ui.cn")
    ↓ Asset ID (业务代码唯一可见物)
Renderer (DoorRenderer / SpriteRenderer / GameRenderer / AudioServer)
```

业务代码目标态示例 (示意, 非 API 承诺):
```cpp
// door_renderer.cpp (现在: 硬编码 4 条 vendor 路径)
Texture2D t = ResourceManager::inst().tex("door.closed");
```

## 4. Manifest schema v2 (向后兼容)

现有 `sprites` 键保留原语义; 新增 `assets` 段承载域化 ID。**不迁移现有 43 个实体 key 的书写方式** (它们已是 ID), 仅补字段:

```json
{
  "version": 2,
  "sprites": { "mon_slime": { "file": "...", "frame_w": 16, "frame_h": 16,
                              "source": "self", "scale_base": 16 } },
  "assets": {
    "door.open":   { "class": "sprite", "file": "assets/vendor/kenney_tiny_dungeon/Tiles/tile_0003.png",
                     "source": "kenney_tiny_dungeon", "render": {"scale_base": 16, "filter": "point"} },
    "door.closed": { "class": "sprite", "file": ".../tile_0022.png", ... },
    "door.locked": { "class": "sprite", "file": ".../tile_0018.png",
                     "render": { "overlay": "#B03030", "overlay_alpha": 0.35 } },
    "door.sealed": { "class": "sprite", "file": ".../tile_0018.png", ... },
    "sfx.timestop":     { "class": "audio", "file": "assets/jojo_timestop.mp3",
                          "fallback": "synth:bolt" },
    "font.ui_cn":       { "class": "font", "file": "assets/fonts/sourcehansans-subset.otf",
                          "fallback": "system:simhei,msyh,simsun", "sizes": [32, 18] }
  }
}
```

D3 落法: `door.locked` 独立 ID + `overlay` 渲染规则 (色罩由 DoorRenderer 按 render 规则绘制, 贴图可先用 tile_0022, 纯清单改动即可见效, 后续换图不改代码)。

## 5. Fallback Policy 矩阵 (按资产类)

| 类 | 缺失降级序列 | 现状 | 变化 |
|----|-------------|------|------|
| sprite (实体) | 文件 → procedural_sprite (色键生成) → 几何回退 | ✓ 已有三级 | 声明进清单 `"fallback": "procedural"` |
| sprite (门/地形) | 文件 → procedural_tile (base 色) | ✓ 已有 | 同上 |
| audio sfx | 文件 → synth:`<key>` → 静默 | ✓ 已有 (timestop 先例) | 全 sfx 统一声明 |
| font | 文件 → 系统链 → GetFontDefault(英文) | ✓ 已有 | D4: 首选改为仓库内开源字体 |
| VFX | procedural_fx → DrawCircle | ✓ 已有 | 不动 (D5) |

原则: **每类资产必须有声明式 fallback**, 禁止"缺失即崩溃"路径进入代码。

## 6. 文件级影响分析 (实施时逐批落地, 本批不动)

| 文件 | 变更 | 规模 | 批次 |
|------|------|------|------|
| `resources/sprites.json` | schema v2: 43 个既有条目补 `source/scale_base`; 新增 `assets` 段 (door×4, sfx×2, font×1) | 数据 | B1 |
| `resource_manager.h/.cpp` | `tex(id)`/`sfx(id)` 清单查询 API (内部复用 `_sprite_defs/_texture_cache`); 不删 `load_texture/sprite_by_key` | ~60 行 | B1 |
| `tests/world/asset_manifest_test.cpp` | **新增** — 完整性校验 (见 §7) | ~120 行 | B1 |
| `door_renderer.cpp` | 4 条硬编码路径 → `tex("door.open|closed|locked|sealed")`; LOCKED overlay 绘制 (D3) | ~30 行 | B2 |
| `audio_server.cpp` | 2 条 MP3 字面量 → 清单 ID; synth fallback 声明化 | ~15 行 | B3 |
| `resource_manager.cpp` 字体链 | 候选表来源改清单 `font.ui_cn` (D4 开源字体入库: assets/fonts/) | ~25 行 | B3 |
| 其余业务代码 | **零改动** (实体侧已经走 ID) | 0 | — |

## 7. 校验测试设计 (B1 交付, 红线即测试)

`asset_manifest_test` (headless, 纯 JSON+磁盘, 无窗口依赖):
1. **可解析**: schema v2 解析成功; 必填字段齐全 (file/source/fallback)
2. **零缺失**: 每个非 `synth:`/`system:` fallback 的 `file` 必须存在于磁盘
3. **零孤儿**: `assets/sprites/` 下每个 PNG 必须被清单引用
4. **ID 规范**: key 匹配 `<域>.<名>` 小写规范; 无两 ID 指向同一文件组歧义
5. **缩放红线**: `scale_base ∈ {16}`; 实体 sprite 帧尺寸 ×2 ∈ {32} (D2 红线机器化)
6. **fallback 完备**: 每条目声明 fallback 且目标存在 (synth key 在合成表中 / 系统链非空)
7. **门语义**: `door.open/closed/locked/sealed` 四 ID 存在; locked 与 closed 渲染规则可区分 (D3 机器化)

## 8. 批次计划 (每批: 改→Build→CTest→提交)

| 批 | 内容 | 风险 |
|----|------|------|
| B1 | schema v2 + 清单查询 API + manifest 校验测试 (TDD: 先写测试) | 低 — 纯增量, 旧 API 不动 |
| B2 | 门迁移 + D3 overlay | 低 — 视觉变化仅 LOCKED 门 |
| B3 | 音频/字体清单化 + D4 开源字体入库 (需下载思源 Subset, 体积评审) | 低-中 — 字体许可/体积需你过目 |

## 9. 明确不做 (Non-Goals)

- 不做热重载/打包/异步加载 (无 profiler 证据支持)
- 不动 `load_texture/sprite_by_key` 既有签名 (零破坏)
- 不把 vendor 闲置资产移入清单 (D6: 保持 Vendor Candidate, 清单外)
- 不引入新第三方库 (nlohmann/raylib 既有能力足够)

## 10. Review Gate 检查单

- [ ] 六要素字段设计是否认可 (§2/§4)
- [ ] door.locked = ID + overlay 方案 (D3 落法)
- [ ] B1-B3 批次切分与顺序
- [ ] 校验红线 1-7 是否有异议
- [ ] D4 开源字体选型预授权 (思源黑体 Subset ~1-2MB 入库)
