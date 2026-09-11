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

## v2 路线 (按优先级)

1. **bloom + 景深 shader** (rlGLSL; raylib 5.0 无 rlights 需自带 shader 文件)
2. 真阴影 (深度纹理 or blob shadow 贴图)
3. 墙/地板 tile 贴图接入 (procedural_tile 拉伸采样, 需换 shader 平面)
4. 特效 additive 混合 + 粒子 billboard 化
5. 完整面板/小地图/传送门 3D 化 (v1 切片仅 HUD)

## 一致性验证协议 (每次改 rendering3d 必跑)

1. 重建 + 60/60 ctest + world_validator
2. `--sim 12 --sim-seed 3` 双跑 → **剔除启动时间戳行**后逐行对比
   (教训: 哈希对比必须排除 "启动" 行, 否则必假阳性)
3. 对照基线: RNG-002 修复后 `reports/p1c8/fix/fix_1_a.out` (061A2BE6)
4. `--sim 2 --sim-seed 3 --hd2d` 冒烟: sim 无头不进 3D, 输出应正常

## 已知限制 (v1)

- 帧动画未接 (billboard 固定 frame 0)
- flip_x 未生效 (DrawBillboardRec 需负宽源矩形, 待 v2)
- 门/楼梯/物品/投射物/传送门未进 3D 分支 (2D 面板在 3D 模式下不画)
- 后处理是叠加矩形近似, 非 shader
