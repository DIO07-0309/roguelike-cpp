# v0.8.0 — Architecture Freeze

## Release Metadata

| Field | Value |
|-------|-------|
| Version | v0.8.0 |
| Codename | Architecture Freeze |
| Date | 2026-07-17 |
| Phase | G1-G3 Complete |
| Status | Stable Baseline for G4 |

---

# v0.9.0 — C++/Python Dual Sync (G5-G6)

| Field | Value |
|-------|-------|
| Version | v0.9.0 |
| Codename | Dual Sync |
| Date | 2026-07-21 |
| Phase | G5-G6 Complete |
| Status | Current Release |

# v0.9.1 — Boss Combat Hardening + Online Adaptive Mirror AI (2026-08-04)

# v0.9.5 — 数据驱动素材接入: Kenney Tiny CC0 精灵上线 (2026-08-06)

## CC0 美术素材落地 — 程序化占位正式被替换
- 素材源: **Kenney "Tiny Dungeon" (CC0 地牢砖块)** + **Clint Bellanger "Tiny Creatures" (CC0 精灵扩展, 16×16 与 Tiny 系无缝兼容)**, 原料库入 `assets/vendor/` (330 文件 + License)
- 工具链: `extract_chars` 同族 Python 辅助 — 从图集按 (col,row) 抠出 17 个精灵 (RGBA), 装饰类剥背景色变透明, 墙/地板保留实心无缝
- 选定精灵: 玩家毒/冰/火三元素形象 (t16/t17/t18)、史莱姆/哥布林/炸弹/坦克/冲锋/召唤师、Boss F5/F10、墙 t040/地板 t049/宝箱/泉水上下/祭坛
- **数据驱动管线**: `resources/sprites.json` (snake_case) → `ResourceManager::load_sprite_config()` (load_all 挂载) → `sprite_by_key(key, def)` — 三态 fallback **素材精灵 > 程序化占位 > 几何回退**
- `SpriteDef.path` 由 `const char*` 改 `std::string` (默认 "" = 程序化占位), 管线统一
- 玩家: `element.type` (FIRE/ICE/POISON) 映射三形象; 怪物: `MonsterType`/名字 → key; GameMap: 墙/地板全部 tile 走素材纹理
- 冒烟运行 5s 无崩溃 · Release 100% · World Validator 0 错误 · 桌面版已同步重编译

# v0.9.4 — 怪物差异化 + 待机帧动画 (2026-08-05)

## 像素管线补全角色辨识度
- `SpriteRenderer::gen_pixel_sprite` body 生成升级为 **2 帧 spritesheet** (32×64: 待机/呼吸), 经 `_blit_frame` (RGBA8 行拷贝, raylib 5.0 无 ImageDrawImage) 拼帧; 呼吸帧亮度 +18 — 与 `frame_rect` 管线直通, 真素材到位仅改 `frame_count`
- Player/Monster 绘制处新增待机帧轮换 (`(int)(GetTime()*4)&1`), `SpriteDef.frame_count=2`
- **怪物差异化体型** (variant 3-6): Charger=箭形三角+冲刺亮条, Tank=方甲+头盔+甲缝, Bomber=圆身+引信火花, Summoner/Shaman=尖帽法袍+水晶; 映射 `_sprite_variant_for(is_boss, MonsterType, name)` 与形状层解耦 (SpriteRenderer 不依赖 game 枚举)
- `_brighten()` 亮度工具替代原先发带的 std::clamp 内联计算
- 验证: Release 100%, 4s 冒烟运行无崩溃, 桌面版已同步重编译

# v0.9.3 — 渲染管线闭环: 角色/怪物/VFX 全接入 SpriteRenderer (2026-08-05)

## 像素管线的圆心落在实体与特效
- `SpriteRenderer::gen_pixel_sprite(body, accent, variant, eye_dir)`: 程序化角色占位 32×32 — variant 0=人形(玩家/普通怪), 1=圆形(史莱姆), 2=大体型(Boss); eye_dir 0下/1上/2左/3右 驱动瞳孔偏移; 头+发带亮条+躯干+噪点+眼
- `Player::draw_no_cam`: 连击段位色(绿→金黄)程序化精灵, 按方向四向占位 (`ply_<dir>_<rgb>` 缓存), 保留阴影/重击放大/Combo 数字, 缺纹回退原几何绘制
- `Monster::draw`: 按体型/类型选 variant 程序化精灵 (`mon_<rgb>_<variant>`), 保留 Boss 光晕/Bomber 脉冲/Tank 边框/Charger 箭头/Summoner 光环/血条等全部功能标记; Boss 继承自动升级
- `SpriteRenderer::gen_pixel_blast(c)`: 程序化 VFX 爆点 32×32 (8 向放射线+中心白核+噪点)
- `GameRenderer::draw_effects`: spark/flash 分支改走爆点纹理 (`fx_<rgb>` 缓存 + tint 淡出缩放), bolt/slash_arc/cone 等仍几何绘制, 缺纹回退原圆
- 素材位替: 管线闭环验证通过 (Release 100%, 4s 冒烟运行无崩溃); 素材到位后 `SpriteDef.path` 即插即用

# v0.9.2 — M4f 美术管线骨架 (2026-08-05)

## 像素渲染管线 (Dark Pixel Fantasy 起点)
- 新增 `src/game/rendering/sprite_renderer.h/.cpp`: `SpriteDef` (path/帧尺寸/帧数) + `SpriteRenderer` (frame_rect/draw_sprite/gen_pixel_tile) — 素材就位后管线零改动
- ResourceManager: `load_texture()` 文件纹理缓存 (失败占位) + `procedural_tile()` 程序化像素纹理缓存 + unload 扩展
- GameMap: `set_palette()` biome 调色板注入 (值拷贝, nullptr 安全) — 墙/地板改用程序化像素纹理 (基色噪点+砖缝/接缝), 缺纹退回几何矩形
- GameScene.enter_floor: biome → 地图调色板 (三 Biome 各自色偏)

## Boss 战斗六大 Bug 修复 (F10/F15)
- BUG 1 UAF: `on_core_maybe_erased()` 钩子 + DOMAIN_PHASE 空核心路径 + reset 清理
- BUG 2 镜像 VFX 禁用: BossSystemDirector 透传 `effects` 通道
- BUG 3 ENRAGED_PHASE 实装: 狂暴攻击×1.3、周期/弱点窗口减半、震屏+文案
- BUG 4 领域核心追玩家: 惰性 MonsterAI 静态桩 (attack_cooldown=999999)
- BUG 5 数据驱动: vulnerable_duration / weakness_dmg_mult 从 domain_config 读取
- BUG 6 弹幕必中: 弹幕/AOE 距离判定

## M4e — 在线自适应 Mirror AI (Thompson Sampling)
- 新增 `src/ai/mirror/online_adaptive_policy.h/.cpp`: contextual bandit (9 上下文桶 × 4 动作臂), Marsaglia-Tsang Beta 采样, 画像先验注入
- MirrorAgent: `recommend_action()` (Phase≥2 接管) + `report_outcome()` (命中/落空反馈)
- MirrorCombatDirector: 决策接管 + 命中/闪避(位移>200px)反馈回路, `_apply_online_action` 动作映射
- 冷启动知识: 玩家习惯画像 → Beta 先验; 战斗中实时纠正
- **跨对局记忆**: Beta 参数持久化到 `saves/save.json` (`mra`/`mrb`), 旧后验叠加为新先验, 镜像跨局累积适应玩家 — 对标觉悟人机"累计学习"
- **玩家技能上下文**: `Player._last_skill_time` 记录技能施放, `player_using_skill` 实装; 技能窗口 40% 探索性反制 (Thompson 决策) + 观察期即时打断 (`should_interrupt_skill(st)`) — 不扩桶保存档兼容
- **日志收敛 + 学习可视化**: 决策/反馈日志降 `LOG_DEBUG`; 镜像面板下方新增"在线学习"HUD — 实时显示上次决策臂 + 当前桶 4 臂胜率进度条 (`_draw_mirror_learning`)

## G5 (C++ Sync)
- 5 new skill behavior classes: IceNova, ChainLightning, ShadowStrike, BloodFrenzy, SummonSpirit
- AIArchetype (4 types: Sniper/Controller/Ambush/Guardian) + MonsterSkillType (12)
- Boss Phase2 (6 unique: Whirlwind/LaserBarrage/GravityPull/etc.)
- BuildType 6→12 (Ice/Fire/Poison/Time/Support/Projectile/IceMage/LightningMage/BleedBlade/ShadowStriker/Juggernaut/SummonLord)
- 10 JSON 100% C++ parity (buffs 25, relics 63, enemies 31, bosses 6, skills 20, items 36, quests 12, dialogues 34, endings 5, meta 10)

## G6 (Architecture)
- EventBus (30 event types, pub/sub)
- ReplaySystem (Record + Playback + StateHash)
- SimRunner (Automated balance testing, --sim N)

## G5.8 (Presentation Layer — 4 commits)
- **BuildTheme**: 7-field struct, 12 presets, 3-tier dmg_color_for()
- **VFX Recipes**: vfx_recipes.json — 12 recipes, 11 color presets, play_recipe()
- **Camera**: shake/dash offset/boss landing zoom
- **Audio Director**: crossfade, boss Phase2 cue, BGM ducking
- **Timeline**: delay/duration/callback sequenced events + include()
- **PresentationEvent + dispatch()**: unified pipeline, Gameplay→Presentation fully decoupled
- **Timeline Presentation**: 12 recipes with staged delays (IceNova: ring→explosion→shatter→flash, Boss Phase2: freeze→flash→roar→shockwave→zoom)

## Python Edition

桌面版同时包含 `python_edition/` 目录，含完整 Python/pygame 源码。
启动方式：`python_edition/main.py`（需 Python 3.11+ + pygame）。

---

## Original v0.8.0 below

## M4a 系列 — Boss 核心环革新 (C++ 版)

| Milestone | 内容 | 状态 |
|-----------|------|------|
| M4a | 暗影骑士连招机器: combo 驱动 (弹幕/扇形斩/瞬移/旋风/召唤) + BossSkillQueue + 技能预警 + zone 修正 | ✅ |
| M4a.1 | 战斗体验修复: 连招触发距离 48→192px / 脱战 384px / 旋风范围圈 / 狂暴演出 / 弹幕特效 | ✅ |
| M4a.2 | 数值平衡: 毒池 0.5s DOT / 弹幕撞墙消失 / 旋风 1.6× 扇形 1.25× | ✅ |
| M4a.3 | 伤害日志全链路: attack_target 统一标签 + logged_hp 记账去重 + 每帧兜底 + [COMBO] 可见性 | ✅ |
| M4b | 第二章 Boss 领域作战 (茶杯头式) | ⏳ 开发中 |

## Scope

G1 (7 steps) — Architecture Foundation
G2 (5 sub-stages) — Content Pipeline & Data Driven
G3 (5 sub-stages) — Data Framework & Architecture Freeze

## Key Metrics

- 172 source files (h/cpp/json)
- 10 JSON config files, 156 data entries
- 12 Data registry modules with unified API
- 30 EventBus event types
- 4 Directors orchestrating 20+ subsystems
- Save format v3 with backward compatibility
- 7-layer layered architecture
- 10 modules under Architecture Freeze (no-refactor)

## Architecture Documents

- [docs/ARCHITECTURE.md] — authoritative architecture reference
- [README.md] — gameplay bible + progress tracking
- [docs/WORLD_LORE.md] — world lore bible
- [docs/D1_GAMEPLAY_LOOP_DESIGN.md] — core loop design

## Development Bible (Frozen Rules)

1. Runtime/Def separation — Def immutable, Runtime mutable
2. Registry pattern — load/get/get_all/is_loaded
3. Manager statelessness — static methods only
4. EventBus decoupling — Gameplay→EventBus→Presentation
5. Save append-only — add fields, preserve semantics
6. Minimal change — add > modify > delete > rewrite

## No-Refactor List (Architecture Freeze)

Object/Node/SceneTree · InputMap · EventBus/ServiceLocator
CombatSystem damage formula · BossAI state machine
DungeonGenerator (BSP) · GameFlowDirector state machine
SaveManager core format · Player/Monster lifecycle
6 Skill execute() methods

## Next Phase: G4 — Platform & Mod Support

- Mod resource override paths
- JSON schema validation
- Manifest system
- Optional hot-reload

## Target: v1.0.0 — Release Candidate (G5)

- Performance profiling
- Memory audit
- Balance pass
- Automated tests
- Package & deploy
