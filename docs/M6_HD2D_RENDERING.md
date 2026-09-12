# M6-HD2D — HD-2D 3D 表现层切片设计

> 状态: **切片 v1 骨架完成** (2026-09-11)。逻辑层零改动, 默认 2D, `--hd2d` 切换。

## 设计约束 (硬红线)

1. **逻辑层不动**: `src/game/rendering3d/` 只读 GameScene 状态, 禁止写 gameplay 状态
2. **RNG 红线** (RNG-001/002 教训): 3D 路径视觉随机只吃 `visual_rng`, 严禁 `rng()`
3. **sim 无头**: sim 模式不初始化 3D (main.cpp 保证), `--hd2d` 与 `--sim` 可并存
4. **回退安全**: 3D 初始化失败自动回退 2D (`g_hd2d_mode = false`)

## 模块结构

```
src/game/rendering3d/
  hd2d_renderer.h/.cpp      — 单例渲染器: Camera3D(45° 俯视) + 分 kind 绘制 + 后处理
  hd2d_scene_builder.h/.cpp  — GameScene → HD2DDrawItem 纯翻译 (namespace hd2d)
```

数据流: `_render()` 顶部分支 → `HD2DRenderer::render_frame()` → `build_scene()`
提取绘制列表 → 地形(地板/墙) → billboard 实体 → 特效 → 2D HUD 桥 (draw_hud 复用)。

## v1 已实现 (切片骨架)

- 透视相机 50° fovy, 45° 俯角, focus 跟随玩家 (2D x/y → 3D x/z, 高度 y)
- 墙体 = 1.25 格高盒子 + 顶面亮/侧面暗伪光照 (无 shader)
- 地板 = XZ 平面 quad, 纯色 (TileType 分色), fog 压暗 40%
- 玩家/怪 billboard = `DrawBillboardRec` (精灵与 2D 同源: player_default /
  sprite_override→mon_orc 回退), 接地阴影扁片
- 特效 = 脉冲发光片; 后处理占位 = 夜色分级 + 地平雾 (v2 换 shader)
- 视野裁剪: 玩家 ±16x±12 tile

## v2a 已实现 (2026-09-12, 全量完成)

- **地形贴图**: 地板 = rlGL 原语贴地 quad (法线朝上, UV 归一化采样);
  墙体 = 四侧面 rlGL quad + 顶面亮 10% 伪受光。贴图回退链与 2D 同源:
  `wall_<biome>` → `wall` → `procedural_tile` (builder `_resolve_tile_tex`)
- **billboard 帧动画**: `((int)(GetTime()*4))&1` 呼吸 2 帧 (与 2D 实体同款,
  非随机不触 RNG 红线); `flip_x` 负宽源矩形生效
- **地面物品/NPC billboard**: item_icon_key / npc_sprite_key 同源图标,
  可见性判定与 2D 一致 (缺素材跳过, 2D 有几何回退 3D 无)
- **挑战传送门**: PORTAL_RING 竖立脉冲双环 (DrawCircle3D) — 入口蓝/返回绿,
  脉冲频率 3.0 与 2D 同源; builder `_build_portals` 条件与 2D 分支逐条对齐
- **UI 尾段共用**: `_render_ui_tail(sw,sh)` — 2D 分支 370 行 UI
  (红屏/黑屏/HUD/小地图/面板/对话/事件/冻结/演出) 单一真相源, 3D 桥同调
- **HUD 参数补齐**: echo 面板 + 挑战波次 (2D 降级参数 nullptr/-1/0 修复;
  `_build_echo_panel_data()` 共用方法)
- **3D 世界标签**: `world_to_screen()` 投影 → 怪名条 + E 对话/拾取气泡
  (屏幕空间绘制, 样式与 2D 同款)

## v2b 已实现 (2026-09-12, 战斗表现层全量)

- **投射物**: PROJECTILE_BODY 发光弹体 (穿透金/元素色/归属色三态) +
  WARNING 相贴地预警环 (AOE) / 轨迹预警线 (点弹, 撞墙截止与 2D 同算法)
- **伤害飘字**: `_render_damage_text` 共用样式, 3D 走 `world_to_screen` 投影
  (暴击 1.6x 字号 / 元素标签 缓冻毒暴 全对齐)
- **射程指示环**: NUNCHAKU 双环带 / SPEAR+CROSSBOW 单环 (暖金, 2D 同源)
- **Boss 技能预警** (只读 BossAI): 弹幕弹道 + 蓄力扇形 (CONE_FAN rlGL
  三角扇, 实时朝向玩家) + 扇形斩 / 瞬移落点紫圈 / 旋风范围圈
- **Boss 战场危险区**: 岩浆/影墙/虚空 贴地危险圈 (warn/active 双态)
- **弱点光环 + Tank 守护连线**: WARNING_RING / ENTITY_LINK
- **空心环原语**: `_draw_flat_ring` (rlGL RL_LINES) — 预警环/射程环底座
- v2a 修复: 传送门环朝向 (竖立朝相机, 原为平躺)

## v2c 已实现 (2026-09-12, shader 档全量)

- **平滑距离雾**: `hd2d_fog.fs` + `hd2d_world.vs` — 克隆 rlgl 默认采样管线
  + viewPos→片元距离 smoothstep 雾色混合; 替代逐 tile 40% 阶跃压暗
  (FOV 探索语义仍由 builder tint 保留; fogStart=520/fogEnd=900 常量起步)
- **Blob shadow**: `GenImageGradientRadial` 64x64 程序纹理贴地椭圆 quad
  替换黑扁片 cube (失败回退原实现)
- **岩浆动画材质**: `hd2d_lava.fs` — 世界坐标 value-noise (暗壳/亮流/热核
  三层分段) + 双向 UV 流动 + emissive 呼吸 (频率 4.0 与 2D 同源);
  LAVA tile 由 builder 打 `is_lava` 标记分流, 探索压暗编码进 tint 灰度
- **Bloom**: 亮部提取 (1/4 RT) → 9-tap 高斯乒乓 (水平/垂直) →
  additive 全屏叠加; 三 GLSL (`hd2d_bloom_extract/blur/composite.fs`,
  composite 降级为 overlay 叠加未用 shader)
- **性能**: 地形两遍分区 (lava 单批 + 其余单批), 每帧仅 2 次 shader 切换
- **嵌套 RT 恢复** (raylib 5.0 坑): `EndTextureMode` 盲绑 FBO 0 并重置
  投影 → `HD2DPostFX::process` 尾部手动恢复 FBO + viewport + 960x640
  ortho 投影; `SceneTree::main_target()` 新增只读 getter
- 新模块: `hd2d_shader_bank` (GLSL 懒加载/缓存/回退标记) +
  `hd2d_post_fx` (bloom 链); GLSL 放 `assets/shaders/` (随 POST_BUILD 拷贝)

## v2d 已实现 (2026-09-12, 战斗反馈打磨)

- **投射物拖尾**: 反速度 3 段渐隐线 (0.03/0.06/0.09s, 递减 50/33/17%,
  BLEND_ADDITIVE) — 2D 穿透弹 back 线 (0.03s) 的 3D 加强版; `trail_dir`
  = p.vel 直接传入 (纯视觉无状态, 不加弹实例 id)
- **名条遮挡裁剪**: `GameMap::has_line_of_sight` (Bresenham tile 步进,
  只读; 越界按挡视线保守处理) — **2D/3D 名条同条件**加玩家→怪视线判定,
  墙后名条不再穿透显示 (怪本体绘制不受影响; 仅 isVisible 层为原语义)

## v2e 已实现 (2026-09-12, shadow map + 点光源 + 打磨)

- **Shadow map**: `hd2d_shadow_caster` 新模块 — rlgl 原语 depth-only fbo
  (480x320, rlLoadFramebuffer+rlLoadTextureDepth+rlFramebufferAttach);
  45° 方向光正交投影 (跟相机焦点), 只画墙 (billboard 实体走 blob 回退,
  alpha 几何全投影不进深度 pass); 地形 shader `sample_shadow` PCF 3x3
  软化 + 环境光 45% 保底; shadow map 成功时 blob alpha 减半
- **点光源**: 地形 shader uniform 数组 (上限 8) — LAVA tile 自发光暖橙
  (半径 3 tile, 上限 7) + 玩家随身火把感暖光; 只读 _draw_items 收集
- **3D 相机 shake**: 2D shake_offset 同源值 → camera focus 偏移
  (set_camera_shake 注入, render_frame 消费后清零)
- **氛围粒子 3D 化**: AMBIENT_MOTE — AmbientLayer::particles() 只读视图
  → additive 微光球 (life 渐隐; 火山余烬/深渊幽光/监狱尘埃的 3D 对应)
- **修复 (systematic-debugging 结案)**: render_depth 曾恢复绑定到
  FBO 0 (屏幕) 而非 scene_tree 主 RT → 主场景画到屏幕 FBO 被 blit
  丢弃 (全屏 (2,2,5) 近黑); 根因 = raylib 5.0 无当前 FBO 查询 API,
  恢复目标需 caller 注入 (render_frame 传 main_target().id)

## v2f 已实现 (2026-09-13, 实体剪影投影)

- **Billboard 进深度 pass**: alpha-discard 深度 shader `hd2d_depth.fs`
  (纹理 alpha*顶点色*diffuse < 0.5 → discard, 像素画硬边缘剪影; 与
  hd2d_world.vs 配对, mvp 由 rlgl 绘制时自动写入 = 光空间矩阵);
  shader_bank 懒加载/回退标记复用 — 编译失败仅墙投影, blob 兜底
- **深度几何同源**: `_draw_billboard_depth` 复用 `DrawBillboardRec`
  顶点公式 (含 flip_x 负宽源矩形); 相机参数只参与朝向数学,
  顶点世界坐标被光空间矩阵变换 — 与主 pass 几何一致
- **当帧相机注入**: `update_light_camera(focus, view_camera)` 存主相机;
  render_frame 相机定位提前到深度 pass 前 (不吃上帧残值)
- **blob shadow 三态降级**: 剪影投影生效=40 / 仅墙=60 / 全回退=120
- **无贴图实体跳过深度 pass**: DrawCube 回退几何 (罕见路径) 不投影,
  blob 兜底; `_draw_scene()` 签名简化 (无参)
- 实机键链验证 (PostMessage 注入): hd2d_depth 编译链接成功 + 深度
  pass 稳定运行, 全 shader 链零 WARN (N→ENTER→SPACE→移动进局)

## v2 剩余路线

(评估中 — v2g 候选: 岩浆点光密度提升 / HD2D 描边重评估 /
bloom 参数场景自适应)

## 已知限制 (v2f 后)

- 雾对 LAVA tile 不生效 (岩浆自发光, 不入雾; 视觉可接受)
- 实体本身不接收阴影 (billboard 走默认管线; v2e 起即如此, 非回归)
- bloom 阈值/强度为全局常量, 未按场景亮度自适应
- 拖尾为直线渐隐 (高速弹转向时无弧度; 弹道本身直线, 语义一致)

## 一致性验证协议 (每次改 rendering3d 必跑)

1. 重建 + 60/60 ctest + world_validator
2. `--sim 12 --sim-seed 3` 双跑 → **剔除启动时间戳行**后逐行对比
   (教训: 哈希对比必须排除 "启动" 行, 否则必假阳性)
3. 对照基线: RNG-002 修复后 `reports/p1c8/fix/fix_1_a.out` (061A2BE6)
4. `--sim 2 --sim-seed 3 --hd2d` 冒烟: sim 无头不进 3D, 输出应正常
