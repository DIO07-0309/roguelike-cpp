# v1.4.25 — M6-v2g 完成: LAVA 点光聚类 + bloom biome 自适应 (2026-09-13)

> v2g 两块视觉小活收官: 火山层岩浆光照从"前 7 块"变"全区均匀亮";
> bloom 参数首次按场景分档 (此前一直是默认常量)。
>
> ## 渲染 (rendering3d)
> - **LAVA 点光网格聚类**: 4-tile 网格分桶每桶 1 光 (桶内首 tile
>   锚点), 光源数与岩浆 tile 总数解耦 (恒 ≤7); 半径 3→3.5 tile
> - **bloom 三档 biome 预设**: 监狱 0.60/0.25/0.58 (低阈值补亮) /
>   深渊 0.68/0.22/0.50 (幽紫光晕) / 火山 0.80/0.15/0.42 (压强度
>   防泛红); set_params 首次接线
>
> 已知限制: bloom 三档为手调常量, 待人眼验收微调。
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳, 与基线一致) + --sim 2 --hd2d 冒烟 exit 0 + 实机键链
> (PostMessage 注入, 全 shader 链零 WARN, 渲染循环稳定)。

# v1.4.24 — M6-v2f 完成: 实体剪影进 shadow map (2026-09-13)

> v2e 只投影墙, billboard 实体走 blob 回退 ("墙有影怪没影") — 本切片
> 补齐实体投影。alpha-discard 深度 shader + 深度几何与主 pass 同源。
>
> ## 渲染 (rendering3d)
> - **Billboard 深度剪影**: hd2d_depth.fs (alpha<0.5 discard, 透明像素
>   不写深度) + hd2d_world.vs 共享顶点; shader_bank 回退机制复用
>   (编译失败仅墙投影, 不崩溃)
> - **几何同源**: _draw_billboard_depth 复用 DrawBillboardRec 顶点公式
>   (含 flip_x), 相机只参与朝向数学 — 深度剪影像素 = 主 pass 可见像素
> - **当帧相机**: update_light_camera 注入主相机; render_frame 相机
>   定位提前 (深度 pass 不吃上帧残值)
> - **blob 三态降级**: 剪影生效=40 / 仅墙投影=60 / 全回退=120
> - 实机键链验证 (PostMessage 注入 N→ENTER→SPACE→移动): hd2d_depth
>   编译成功 + 深度 RT 480x320 就绪 + 全 shader 链零 WARN
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳, 与 RNG-002 基线一致) + --sim 2 --hd2d 冒烟 exit 0。

# v1.4.23 — M6-v2e 完成: Shadow map + 点光源 + 打磨 (2026-09-12)

> v2e 合体方案落地。HD2D 风格核心拼图: 方向光阴影 + 岩浆/火把点光。
> 附带 3D 相机 shake 接线 + 氛围粒子 3D 化。修复深度 pass FBO 恢复 bug。
>
> ## 渲染 (rendering3d)
> - **Shadow map**: hd2d_shadow_caster 新模块 — rlgl 原语 depth-only
>   fbo (480x320); 45° 方向光正交投影跟相机; 墙几何深度 pass;
>   地形 shader PCF 3x3 软化 + 环境光 45% 保底; blob shadow 在
>   shadow map 激活时 alpha 减半 (接地感保留)
> - **点光源**: LAVA tile 暖橙自发光 (半径 3 tile × 7) + 玩家火把暖光;
>   距离平方衰减; shader uniform 数组只读收集, 逻辑层零改动
> - **3D 相机 shake**: set_camera_shake 注入 (2D shake_offset 同源,
>   RNG 红线语义保留 — 独立视觉流)
> - **氛围粒子 3D 化**: AMBIENT_MOTE additive 微光球 (AmbientLayer
>   particles() 只读视图; 火山余烬/深渊幽光 3D 对应)
> - **修复**: render_depth FBO 恢复 bug — 曾绑回 FBO 0 (屏幕) 而非
>   scene_tree 主 RT, 主场景被 blit 丢弃 (截图全屏近黑;
>   DEBUG 常量色 shader 分层隔离定位, caller 注入 outer_fbo 结案)
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 零分岔 + --hd2d
> 实机键链 (shader/shadow RT 全加载) + 像素分布回归正常
> (avg 57,62,75 / 纯黑 0.2%, 修复前全屏 (2,2,5))。

# v1.4.22 — M6-v2d 完成: 战斗反馈打磨 (拖尾 + 名条遮挡) (2026-09-12)

> v2d 小步打磨。3D 投射物拖尾 + 2D/3D 同条件名条视线裁剪。
>
> ## 渲染 (rendering3d) + 世界 (world)
> - **投射物拖尾**: _draw_projectile_trail — 反速度 3 段渐隐线
>   (0.03/0.06/0.09s, 递减 50/33/17%, BLEND_ADDITIVE); 语义对齐 2D
>   穿透弹 back 线 (0.03s) 并加强; trail_dir=p.vel 直接传递, 零状态
>   (不加弹实例 id, 不触碰逻辑层)
> - **名条遮挡裁剪**: GameMap::has_line_of_sight 新增 (Bresenham
>   tile 步进, 只读, 端点不检查, 越界保守挡视线); 2D 名条与 3D 投影
>   名条同条件判定 — 墙后名条不再穿透 (怪本体绘制不变)
>
> 验证: 60/60 ctest + validator 0 err + sim 12×2 双跑零分岔 +
> --hd2d 实机 (键链自动进游戏) shader 全加载 + 优雅退出 exit 0。

# v1.4.21 — M6-v2c 完成: 3D 表现层 C 档 shader 全量 (2026-09-12)

> v2c shader 档收官。距离雾/blob 阴影/岩浆动画材质/bloom 四项落地,
> 全部带失败回退 (shader 编译失败自动降级默认管线, 不崩溃不黑屏)。
>
> ## 渲染 (rendering3d)
> - **平滑距离雾**: hd2d_fog.fs + hd2d_world.vs — 克隆 rlgl 默认采样管线
>   + viewPos→片元距离 smoothstep 雾色混合; 替代逐 tile 40% 阶跃压暗
>   (FOV 探索语义仍由 builder tint 保留)
> - **Blob shadow**: GenImageGradientRadial 64x64 程序纹理贴地椭圆
>   替换黑扁片 cube (_draw_blob_shadow 提取, 失败回退原实现)
> - **岩浆动画材质**: hd2d_lava.fs — 世界坐标 value-noise 三层分段
>   (暗壳/亮流/热核, 对齐 2D LAVA 深底/裂纹/热核) + 双向流动 +
>   emissive 呼吸 (频率 4.0 同源); builder is_lava 标记分流,
>   探索压暗编码进 tint 灰度
> - **Bloom 后处理链**: 亮部提取 (1/4 RT, Rec.709 亮度阈值+平方衰减) →
>   9-tap 高斯乒乓 (水平/垂直) → additive 全屏叠加 (BLEND_ADDITIVE)
> - **性能**: 地形两遍分区 (lava 单批 + 其余单批), 每帧仅 2 次 shader
>   切换, 避免逐 tile 切换的数百次 batch flush
> - **raylib 5.0 嵌套 RT 坑**: EndTextureMode 盲绑 FBO 0 并重置投影 →
>   HD2DPostFX::process 尾部手动恢复 FBO+viewport+960x640 ortho;
>   SceneTree::main_target() 新增只读 getter
> - 新模块: hd2d_shader_bank (GLSL 懒加载/缓存/回退标记) +
>   hd2d_post_fx (bloom 链); GLSL 6 文件放 assets/shaders/
>   (随 POST_BUILD assets 拷贝, 桌面镜像自动覆盖)
>
> 已知限制: 雾不作用于岩浆 (自发光); blob shadow 非形状阴影
> (shadow map 列 v2d); bloom 参数为全局常量未自适应。
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔
> (剔时间戳) + --sim 2 --hd2d 冒烟 exit=0 无 WARN/ERROR/FATAL。

# v1.4.20 — M6-v2b 完成: 3D 战斗表现层 B 档全量 (2026-09-12)

> v2b 切片收官。战斗反馈全链 3D 化: 投射物/预警/飘字/技能几何/危险区,
> 3D 模式下战斗信息不再缺席。逐项对齐 2D 配色与触发条件 (只读翻译)。
>
> ## 渲染 (rendering3d)
> - **投射物三态**: PROJECTILE_BODY 发光双球 (穿透金/敌元素火红冰蓝/玩家
>   土金); WARNING 相 AOE→贴地空心预警环 (红/橙/黄三级 + 脉冲),
>   点弹→TRAJECTORY_LINE 轨迹线 (撞墙截止算法与 2D _preview 同源) + 落点圈
> - **伤害飘字**: _render_damage_text 提取共用样式 (暴击1.6x/元素标签),
>   2D 相机偏移 / 3D world_to_screen 投影两路分发
> - **射程指示环**: WARNING_RING 复用 — NUNCHAKU 双环带 (内环+外环+淡带),
>   SPEAR/CROSSBOW 单环; 暖金色与 2D 同源
> - **Boss 技能预警** (只读 BossAI): 弹幕在飞弹道线 + 蓄力扇形预警
>   (CONE_FAN rlGL 三角扇, 朝玩家实时角度); 扇形斩蓄力面 (橙红);
>   瞬移落点紫圈; 旋风蓄力白环/旋转紫圈
> - **Boss 战场危险区**: 岩浆/影墙/虚空 贴地危险圈 (warn橙黄/active红,
>   boss_ctrl() 只读访问器)
> - **弱点光环 + Tank 守护连线**: WARNING_RING 橙脉冲环 + ENTITY_LINK 3D线
> - **空心环原语**: _draw_flat_ring (rlGL RL_LINES 圆周线段) —
>   DrawCircle3D 实心无法挖空心, 预警环全部走原语
> - **v2a 修复**: 传送门环朝向修正 ({0,1,0}/45° 朝相机竖立, 原 {1,0,0}/90°
>   是平躺贴片 — Code Review 抓获)
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 +
> --hd2d 与基线一致 (剔时间戳/HD2D行) + 冒烟 20s 无崩溃。

# v1.4.19 — M6-v2a 完成: 3D 表现层 A 档纯接线全量 (2026-09-12)

> v2a 收尾 (方案一: 3D 世界 + 2D 屏幕空间 UI)。7 项 A 档缺失全部接通,
> 3D 模式功能对齐 2D 的可玩闭环。
>
> ## 渲染 (rendering3d + game_scene 3D 桥)
> - **地面物品 billboard**: item_icon_key 图标 3D 化, 可见性同 2D (缺素材跳过)
> - **NPC billboard**: npc_sprite_key 楼层映射 (npc_views() 只读快照)
> - **挑战传送门**: PORTAL_RING 竖立脉冲双环 (DrawCircle3D, 入口蓝/返回绿,
>   颜色/脉冲频率与 2D 同源) + 地面基准圈
> - **HUD 参数补齐**: echo 面板 (F15 Ending Echo 镜像数据) + 挑战波次全量传入
>   — _build_echo_panel_data() 提取为 2D/3D 共用方法 (buff 腐化名表重构为
>   数据驱动 MAP, _fill_echo_buffs)
> - **UI 尾段共用**: _render() 的 370 行 UI (红屏/黑屏/挑战选择/小地图/背包/
>   赌博/对话/事件/冻结/时停/Boss 演出) 提取为 _render_ui_tail(sw,sh),
>   2D/3D 分支同 UI — 单一真相源, 消除双份维护
> - **3D 世界标签**: world_to_screen() 投影 (GetWorldToScreen);
>   怪名条 (Boss红/精英金/普通灰) + E 对话/拾取气泡, 样式与 2D 同款
>
> ## 结构
> - GameScene 新增只读访问器 (3D 红线): npc_views() / dropped_items() /
>   in_challenge_arena() — rendering3d 无 friend, 无可变访问
> - _render_hd2d_ui_bridge / _render_hd2d_world_labels (≤40行, 拆分合规)
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 (剔时间戳)
> + --hd2d 冒烟 20s 浸泡无崩溃 (每项增量验证 + 收尾全协议)。

# v1.4.18 — M6-v2a 第一刀: 3D 地形贴图 + billboard 帧动画 (2026-09-12)

> v2a 切片 (方案一: 3D 世界 + 2D 屏幕空间 UI) 开工。本刀: 地形/墙体接群系贴图,
> 实体接呼吸帧动画 + flip_x, 全部复用 2D 同源素材回退链, 零 shader 依赖。
>
> ## 渲染 (rendering3d)
> - 地板: rlGL 原语贴地 quad 采样 tile 贴图 (v1 的顶视 billboard 近似退役),
>   无贴图回退 DrawPlane 纯色
> - 墙体: 四侧面 rlGL quad 贴图 + 顶面亮 10% 伪受光; 无贴图回退 v1 纯色盒子
> - 贴图解析: 群系 wall_<biome>/floor_<biome> → 通用 → 程序化 (与 2D
>   GameMap::draw 同链, hd2d_scene_builder._resolve_tile_tex)
> - billboard 帧动画: `((int)(GetTime()*4))&1` 呼吸 2 帧轮换 (与 2D 实体同款,
>   GetTime 非随机, 不触 RNG 红线); flip_x 负宽源矩形接线生效
>
> ## 红线遵守
> - 视觉随机零新增 (帧驱动只用 GetTime); rendering3d 仍只读 GameScene
>
> 验证: 60/60 ctest + world_validator 0 err + sim 12×2 双跑零分岔 (剔时间戳)
> + --hd2d sim 与基线一致 + --hd2d 冒烟 20s 浸泡无崩溃 (HD2D 激活日志确认)。

# v1.4.17 — P1-C9: 3D包输入失灵调查结案 + --input-diag 诊断开关 (2026-09-11)

> 用户报告 3D 包 exe "进层后键盘失灵"。系统化排查后结案: f6019ee 无罪, 环境瞬态。
> 3D 模式入口补齐: HD2D 激活日志 + 桌面包 "3D模式" 快捷方式。
>
> ## 调查结论
> - 代码审查: f6019ee 输入链零改动, `--hd2d` 无参数时为死分支
> - PostMessage 全流程探针 (标题→选档→进层→Esc存档) 原包 exe 全通, 输入链完好
> - 用户复测诊断版: keys=1 到达 GLFW, focus=1 全程, 正常玩到第2层
> - 环境线索: 当日系统日志 VMware hcmon USB 驱动风暴 (3.4万条, 键盘=USB HID),
>   17:08/18:29 失灵会话为驱动层瞬态干扰, 与游戏代码无关
>
> ## 新增
> - `--input-diag` 启动参数: 主循环每 2 秒记录 [INPUT-DIAG] 键盘队列/焦点/
>   鼠标三态到 game.log — 键盘失灵复发时一跑即定位 (失焦/消息不达/状态卡死)
> - SceneTree::set_input_diag(); 默认关, 零日志噪声
> - HD2DRenderer::ensure_init 激活日志 (区分 2D/3D 路径, 排障可辨)
> - 桌面 3D 包根目录新增 "3D模式" 快捷方式 (roguelike_cpp.exe --hd2d)
>
> 验证: 60/60 ctest; --hd2d 启动 3D 激活日志确认; 无参启动 0 条 DIAG。

# v1.4.16 — M6-HD2D 切片: 3D 表现层骨架 (--hd2d 可切换) (2026-09-11)

> 大更新第一步: HD-2D 渲染切片落地。逻辑层零改动, 默认仍是 2D。
>
> ## 新增
> - `src/game/rendering3d/` 模块: HD2DRenderer (Camera3D 45° 俯视 + 分层绘制 +
>   后处理占位) + HD2DSceneBuilder (GameScene 只读状态 → 绘制列表纯翻译层)
> - `--hd2d` 启动参数: 3D 世界层 + 2D HUD 桥; 初始化失败自动回退 2D
> - 墙体盒子伪光照 / 地板分色 / 实体 billboard (精灵与 2D 同源) / 特效脉冲片
> - 设计文档: docs/M6_HD2D_RENDERING.md (含 v2 路线与一致性验证协议)
>
> ## 红线遵守
> - rendering3d 只读 GameScene, 无 gameplay 副作用
> - 视觉随机只吃 visual_rng (RNG-001/002)
> - sim 无头模式不进 3D (--hd2d 与 --sim 并存验证通过)
>
> 验证: 60/60 ctest + world_validator + sim 12×2 与 RNG-002 基线
> **剔除启动时间戳后逐行一致** (哈希对比教训: 时间戳行必假阳性)。

# v1.4.15 — P1-C8 结案: RNG-002 视觉掷骰流污染修复 (2026-09-11)

> v1.4.14 记录的 sim 间歇非确定性 (同 exe 同 seed 12~50% 批次分岔) 根因锁定并修复。
>
> ## 根因 (RNG-002)
> - `vfx_server.cpp` 5 个 VFX 函数 (lightning/explosion/smoke_puff/spark_burst/
>   blood_frenzy) 用 gameplay `rng()` 生成纯视觉粒子参数 — RNG-001 (_add_noise
>   偷吃主 rng 流) 的同族漏网。一次攻击特效 = 50~150 draws, 同帧事件路径差异
>   经此放大成 RNG 流永久错位 → 怪池/id/进层全雪崩
> - 证据链 (RNGSPIKE/RNGSITE 探针 + ASLR slide call 边界唯一确定法):
>   分岔 tick B 独有 117-draw spike, 调用点全解析为 VFX 内联掷骰 + 击杀链;
>   帧末状态哈希仍一致 (纯掷骰差, 无 gameplay 差异) → 流错位后雪崩
>
> ## 修复 (方案 A)
> - vfx_server.cpp 11 处 `rng()` → `visual_rng()` (RNG-001 同法, 独立视觉流)
> - 全部 P1-C8 探针移除 (game_scene.cpp FP/POS/SPD/RNGSPIKE/RNGSITE +
>   combat_system.h CountingRng 返回地址环形缓冲)
>
> ## 验证
> - 60/60 ctest + world_validator 全绿
> - **16 对并行 (32 进程批) --sim 12 --sim-seed 3 零分岔**, 同哈希
>   061A2BE6... (修复前同协议每批 1~4 对分岔)
> - 新基线: avg_floor=7.00 dmg_dealt=1987.2 (旧基线被 RNG-002 污染, 轻微
>   移动属预期; 后续对照以新基线为准)
> - 遗留: combat_coordinator.cpp:72 pre_hp 裸指针快照 (已证非本例根因, 清理候选);
>   "第一信号"上游机理未深挖 (32 批零分岔下未再现, 若复发按 WIP §9.1 方法论重启)

# v1.4.14 — M5 尾批: 共用图清零 — visual_id 数据驱动全量接线 + 潜伏者死规则修复 (2026-09-10)

> V1_4 审计缺口①③清零 (17/30 共用 orc + 3 个 F5 Boss 共用一图)。
>
> ## 修复 (Bug)
> - **潜伏者死规则**: monster.cpp 名字规则匹配"潜行者"但 enemies.json 实名
>   "暗影**潜伏**者" (UTF-8 字节不重合) → mon_shadow_stalker 专属图自 M5-B
>   接线以来从未命中 — 最高覆盖兜底怪一直渲染成 orc。修复 = 1 行 + visual_id
>   接线根治整类问题
>
> ## 视觉改动 (visual_id 数据驱动, 名字规则降级为回退链)
> - **F5/F10 Boss 分图**: 暗影骑士/亡灵法师/血族伯爵/地狱火魔 各获专属图
>   (boss_<visual_id>), Boss 立绘/战场精灵同链路; F15 镜像保持玩家形象
>   (设计意图)。旧 key boss_f5/boss_f10 保留兜底 (title_scene 仍用)
> - **enemies.json visual_id 语义修正 ×20**: 20 只怪的 visual_id 原是
>   "体型模板复用" (dark_mage 顶著 shaman 的 vid), 回归"自身视觉标识" —
>   这是"共用图"的另一半根源
> - **专属图 ×25** (m5_sprite_gen.py 扩展, 全 16x16 统一描边): 哥布林弓手/
>   冰霜史莱姆/电光之核/毒液蠕虫/魔像/亡语者/雷暴元素/血祭司/石像守卫/
>   铁卫/骨兵/骷髅弓手/哥布林猎手/暗术师/虚空行者/夜行猎手/冰狱守卫/
>   鲜血水蛭 + 4 Boss — 30 只怪全部一怪一图, 共用 orc 图时代结束
> - **_visual_to_color 色表扩展 ×20**: visual_id 修正后程序化占位/几何
>   回退的身体色同步专属配色
>
> ## 过程插曲 (诚实记录)
> - 冒烟对照发现 HEAD 预存在**间歇非确定性**: 12 连跑 3 次分岔 (F3 毒 tick
>   时序差 1 条), 同 exe 同种子 — 与本批无关 (分岔行号/模式与改动态完全
>   一致), 记 **P1-C8 候选**。v1.4.13 的"24 连跑一致"结论按间歇检出概率
>   需要重新审视
>
> ## 复核补遗 (M5-E 收尾, 同日)
> - **mon_elite_slime 缺图补齐**: 复核发现 "30 怪一怪一图" 漏了精英史莱姆
>   (visual_id 一直正确但图从未生成, 回退渲染成普通史莱姆)。金冠三尖
>   画法 + sprites.json 注册 — 交叉引用核查脚本现在 mon_ missing = []
> - **demon_lord 不补 (设计意图)**: F15 终焉回响 = 镜像玩家形象 boss_self,
>   visual_id 派生链正确跳过
>
> 验证: world_validator 0 错 + 60/60 测试 + asset_manifest 补测 + 冒烟
> (af=6.65/dmg=1668.8, 与视觉接线前 subset 逐值一致)。

---

# v1.4.13 — M5 视觉批: 三群系贴图 + 兜底怪专属图 + RNG-001 违规修复 (2026-09-10)

> V1.4 Roadmap M5 (数据驱动美术)。程序化生成 (tools/m5_sprite_gen.py,
> 复刻既有 48 张手工像素画的画法规律: 16x16/统一描边/高饱和主色)。
>
> ## 视觉改动
> - **三群系专属 wall/floor 贴图** (V1.4 审计的"最大杠杆"): 监狱 (石砖+青苔)/
>   火山 (玄武岩+熔岩裂缝发光)/深渊 (紫岩+发光符文+幽光裂缝), 全游戏不再一套贴图
>   tint 到底。GameMap 按 biomes.json id 选择, 回退链: 群系图→通用图→程序化
> - **兜底怪专属图 ×4**: mon_shadow_stalker (暗影潜行者, 监狱15%+深渊30% 双
>   出场)/mon_fire_imp (火魔, 火山30%)/mon_elite_orc (精英兽人, 火山25%)/
>   mon_shadow_assassin (暗影刺客, 深渊25%) — 名字规则扩展 (法师/守卫/兽人精英
>   分流), 兜底 mon_orc 覆盖面大幅缩小
>
> ## 修复 (本轮最大意外收获)
> - **RNG-001 违规**: `SpriteRenderer::_add_noise` 程序化纹理噪声吃的是
>   gameplay `rng()` — 素材命中与否 (渲染层差异) 直接污染 gameplay 随机流,
>   造成同 seed 间歇世界线分岔 (~7% 触发, M5 接线把它从潜伏炸到 2/3 概率才
>   暴露)。修复: 改吃 `visual_rng` (G9.3 规范的独立视觉流)。**24 连跑
>   确定性面板全一致**; 修复后冒烟与 P1-C7 结项基线逐值一致 (af=6.75/
>   dmg=1823.4) — 纯渲染层改动零 gameplay 影响, 完美自证
>
> 验证: world_validator 0 错 + 60/60 测试 + asset_manifest_test + 冒烟基线一致。

---

# v1.4.12 — P1-C7-A 结项: 双轨判定统一落地 + Boss reset 语义修复 (2026-09-10)

> 大更新批（四任务一天完成）：空手攻击正式迁入 WeaponExecutor 数据驱动轨。
>
> ## 玩法/系统改动
> - **空手/持械统一判定轨**（T1 B方案）: fist 走 weapons.json 数据驱动
>   (range 1.5=48px/recovery 0.5s 与 legacy 数学等值), 删 player_controller
>   legacy 分支 ~70 行。三败后的成功配方: **节奏保持, 只换实现**。
>   聚合验证 5 种子×100: af 1.60→1.66, dmg 196→201 (小幅偏好, CIRCLE 判定
>   圈内群杀 + executor 暴击梯度贡献)
> - **MCTS 感知统一**（T2）: build_sim_state 改读 weapon.can_attack —
>   空手/持械 AI 感知不再错轨; Player::can_attack/ATTACK_COOLDOWN 死代码删除
> - **Boss reset 语义拆分**（T3/P1-C7-C）: reset()→reset_floor()/reset_run(),
>   enter_floor/new_game 显式调用点补全。WIP "零调用"前提修正 — FLOOR_ENTER
>   EventBus 路径原本就在跑, 修复为零行为差 (逐字节实证)。replay_mem 语义
>   审计: init_on_spawn 每战重建, 无跨层持久项; mirror 跨局记忆走独立通道
> - **spear 局尾 special 残留嫌疑划掉**（T4）: 30 局探针 0 残留
>
> ## 过程插曲
> - T2 双跑分岔假阳性: bisect 排查 + 6 连跑 1 哈希证伪 — P1-C4
>   "间歇判定 N≥4" 手册教训再验证
>
> 验证: 基线复现/T1 冒烟+聚合/T2 6连跑+60测试/T3 逐字节+60测试/T4 探针 全过。
> 结项报告: docs/P1C7A_FINAL_REPORT.md

---

# v1.4.11 — P1-C7-A 会话4: 聚合判决, 迁移三连败坐实 (2026-09-09)

> 判决批：v3 迁移 (fist 48px/0.35s + 删 legacy 分支) 跑满 5 种子×100 局聚合 —
> **全面负回归 (af 1.60→1.30, dmg 196→99, s7 极端形态 dealt=2.1/局)** → 回退。
> 三次尝试 (v1/v2/v3) 翻在同一处: 0.35s 出手节奏改变 AI 决策相位, 而 Q3.15
> 决策权重表按 0.5s 节奏标定 — **迁轨前置条件是先重构决策权重表**。
> 诊断增量: ①chase 再平衡实验 (move 0.65 > attack 分 → AI 永不选 attack,
> 负回归回退) ②"run2+ 零出手"之谜解开 = 速死局果非因 (GATE2 采样缺失,
> 局生命周期 <600帧) ③决策层健康证明 (atk>0 211/288 样本)。
> 新工具: `tools/p1c7a_summary.py` (5 种子聚合器)。路线改道: 第 5 会话
> 推荐 B 方案 (fist recovery=0.5 节奏保持迁移) — 详见 WIP 文档。
> 工作区回退至基线 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.10 — P1-C7-B 泄漏排查: 五假说全灭, 真因定位 (2026-09-09)

> 排查批：镜像冻结/UI/GameState/输入门/executor 门五个泄漏假说逐一探针证伪。
> 真因 = 迁移出手节奏 (0.5s→0.35s/击) 触发"战斗-搜刮时序再平衡"，串行批
> 混沌流中演化为 F1 全灭 — 与 P1-C6 教训同构 (出手时序深度耦合)。
> 附带发现: `BossSystemDirector::reset()` 全仓零调用 (设计意图未兑现) —
> 记 P1-C7-C。工作区回退至基线等价 (af=6.25/dmg=1746.2 复现)。

---

# v1.4.9 — P1-C7-A 会话2: 泄漏实锤 + 顺序调整 (2026-09-08)

> 判决性对照批：单局 ×5 种子证明迁移链路无害 (af 7.2 vs 7.0, 无 F1 灭);
> 串行批 run2 起空手局零出手 + 同 exe 双批逐字节一致 = **确定性批内局间泄漏**
> (基线与 v2 共有, 修它优先于迁移 — 立项 P1-C7-B)。
> WIP 文档大更新: 单局/串行对照表、已排除项、泄漏嫌疑清单 ×5、定位法。
> 工作区回退至 e542af0 行为等价 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.8 — P1-C7-A 双轨判定统一: WIP 交接 (2026-09-08)

> 进行中批：空手 legacy 轨→WeaponExecutor 迁移一版冒烟 run0/1 起飞 (F10/F11)
> 但 run2-19 空手 F1 全灭 → 回退。链路探针已证 executor 命中/节奏正常，
> 遗留谜团与下场清单见 `docs/P1C7A_DUAL_TRACK_UNIFICATION_WIP.md`。
> 工作区回退至 e542af0 行为等价 (af=6.25/dmg=1746.2 精确复现)。

---

# v1.4.7 — P1-C6 冷却感知出手时序实验: 四版负回归拦截 (2026-09-08)

> 方法论批（续 C5）：C5 处方"冷却感知决策"经四版参数/结构实验全部证伪，逐一回退。零行为改动落地。

## 实验与判决 (详见 `docs/P1C6_COOLDOWN_AWARENESS_NEGATIVE_RESULTS_REVIEW.md`)

- **v1 全 ε=0.02 让渡** (af 6.25→6.05, dmg -12%): 挨打窗口全部让渡给
  撤退/喝药 → 生存↑输出↓ 同源置换 (taken -11%, heal -34%)。
  **d3=77% 边缘圈站桩不是纯病理 — 是"最大化输出姿态"**，莽性是当前
  平衡的隐性成分
- **v1.3/v1.4 比例衰减 30%/50%** (af→3.85/1.40, F1 灭 11/18 局): 冷却期
  攻击分低于拾取 1.6/撤退 0.6 → 持械残局"捡逃死循环" (3 次传送零出手)
- **防御性门全部零命中**: special 追击期排除 + 单怪步进门在 s3 冒烟下
  三版 20 局 log 逐字节一致 — 分岔从 run3 起混沌传播
- **步进门是 F1 全灭放大器**: 衰减+步进 af 1.40 vs 只衰减 4.45

## 新发现 (记入 P1-C7 输入)

- **双轨攻击判定**: 空手 legacy 0.5s CD vs 持械 0.15s CD — 冷却感知的
  感知源错轨，统一判定是冷却类方案的**前置修复** (S 工作量)
- **跨 run 世界状态无隔离**: 同 seed 冒烟 run0-2 与基线逐字节一致、
  run3 起分岔 — 单局对比无效，新增"run 边界对齐 + 首分岔定位"方法论
- 回退后基线精确复现: af=6.25 / dmg=1746.2 / d3=77.1% (n=118) ·
  ctest 60/60 · validator 0 错 0 警 — 500 局历史数据延续有效

---

# v1.4.6 — P1-C5 攻击圈对齐实验: 三项负回归拦截 (2026-09-08)

> 方法论批：三项"显然正确"的改动在冒烟数据下全部暴露为负回归，逐一回退。零行为改动落地。

## 实验与判决 (详见 `docs/P1C5_ATTACK_ALIGNMENT_NEGATIVE_RESULTS_REVIEW.md`)

- **决策圈对齐武器半径**：6 版参数扫描全部负回归 (af 6.25→1.35~3.60)。
  核心发现："边缘圈空挥"实为**预判性试挥** — 判定 48px > FIST hit 32px，
  试挥中冷却流转，怪进圈瞬间命中；收窄判定后出手时机被 move 步进抢占。
  **与 Q3.15 风筝同构的时序耦合，判定圈不可单点改**。
- **current_stage 动态段 reach**：同 seed **3 种结局随机** (继 P1-C4 后第二个
  "同 seed 多结局"缺陷)。五层探针收网到 kill #117 目标选择分岔 (同帧同 rng
  序列下 A 杀兽人#142/B 杀史莱姆#143)。回退 stages[0]，根因记 P1-C6 专项。
- **空手武器追击**：F1 怪密度下直线穿怪=送头，理论风险确认；基建保留。

## 落地 (零行为差异, 已验证)

- GroundSpot.is_weapon + `_near_weapon_loot_dist` + `_decision_attack_reach_px`
  + `_is_bare_fisted` — 决策基建四件套
- C4PROBE 相对化刻度 (d×4/reach)：**d3 (75-100% 判定圈) = 77.1%** — AI 攻击
  决策 77% 集中在边缘圈，消它需出手时序层方案 (P1-C6: 冷却感知决策)
- 500 局 log 与 P1-C4 逐字节等价 (13332 行仅 1 行时间戳差) — 基线数据延续有效
- 验证：4 连跑 MD5 一致 · 基线 6.25/1746 精确复现

---

# v1.4.5 — P1-C4 时停 UAF 根治 + 模拟器确定性恢复 (2026-09-08)

> 本批核心产出：发现并修复 P1-C3 全部历史 500 局数据的**地基缺陷**——同 seed 双结局。

## 改动

### UAF 根治 (game_scene.h / game_scene_combat.cpp / player_controller.cpp / combat_coordinator.*)

- **根因**：`pending_damage` 存裸 `Monster*`。时停期间目标怪被 cleanup/kill 路径
  erase 释放 → 结算时悬空。堆地址被新怪复用会**欺骗 valid 检查把伤害打错怪**；
  常规失效则**伤害凭空丢失**（84 点挂起伤害消失 → 兽人不死 → 世界线分裂）。
  是否触发取决于进程堆布局（Windows ASLR）——纯运行期运气
- **修复**：`pending_damage` 改存 `instance_id`，结算按 id 在 monsters 中查找，
  id 失效诚实跳过。删除零调用者的死代码 `CombatCoordinator::apply_pending_damage`
- **验证**：同 seed 8 连跑 MD5 全一致（修复前 ~50% 概率分岔成两种结局）

### SimAI 贴脸拉开分 0.9→0.6 (sim_ai.cpp)

P1-C3 数据：270 局 F1 围殴死 100% 零杀（0.9 分撤退持续压过贴脸攻击 0.67，
全程逃命被咬死）。0.6 让贴脸攻击反超 → "逃一步打一下"轮换。

### C4PROBE 常驻探针 (sim_ai.cpp / sim_runner.cpp)

攻击评分命中距离分布：**d1（32-48px 边缘圈）占 82.2%**，d0 稳定出手区仅 17.8%
——F1 围殴死亡真因定位为"攻击圈边缘站桩"，是 P1-C5 贴脸步进泛化的直接依据。

## 定位过程（五层探针收网，详见 `docs/P1C4_UAF_DETERMINISM_DATA_REVIEW.md`）

传送指纹 → 帧级 FPDIAG → KILLDIAG → 时停/挂起计数 → PENDDIAG（DANGLING 实锤）。
每层一个可证伪假设。**教训：MD5 双跑验证对间歇性缺陷是假阳性，
N≥4 次重复才可信。**

## 500 局对比（P1-C3 → P1-C4）

- 胜利 0 → **1/500**（s19 首胜 ⭐）；F1 死亡 90.2%→91.0%、TWall 7.8%→7.0% 持平
- 判读：本批是**地基修复批**非调优批——数值持平符合预期；
  F1 围殴 91% 仍是最大瓶颈，C4PROBE 已精确到"边缘圈站桩"
- 验证：60/60 ctest · Validator 0 错 · 8 连跑 MD5 一致（s3）+ 4 连跑（s7）

---

# v1.4.4 — P1-C3 楼梯导航 + 层级搜刮预算 (2026-09-07)

> C3DIAG 探针定位"清层不下楼"死锁：AI 返回 descend 但从不导航去楼梯格，站原地按 E 600s。

## 改动 (sim_ai.cpp/h + game_scene.cpp)

- `_bfs_to_stairs`：BFS 导航至楼梯格，在格才返回 "descend"（原 `_check_floor_transition`
  只认"站在楼梯上按 E"——AI 从不走路，9/20 冒烟局困死于此）
- `set_stairs_pos` 每帧只读注入楼梯坐标（`set_ground_items` 同契约）+ 换层重置搜刮状态
- 层级搜刮预算 15s：stairs 激活起计时，超时放弃余下搜刮直奔楼梯
- `_loot_abandoned` 锁定：看门狗触发后不再回头搜刮（原 descend 后又走向搜刮房死循环）
- C3DIAG 常驻探针（墙超时分支快照，与 P0DIAG 同构）

## 方法论记录（本轮核心产出）

- **无进展看门狗传送方案两轮迭代均产生传送风暴**（273 次/20 局）——回退。
  教训：没有复现个案前不写修复，探针先行。
- **混沌重排陷阱**：楼梯修复后 s3 deep 22→5 表面回归；MD5 对拍证明无注入时与
  P1-C2 逐字节一致——差异全部来自"AI 真的下楼了"之后的 RNG 分岔。预算 15s→25s
  实验证明调参不单调——**以结构指标定参，不拟合种子噪声**。

## 500 局对比（P1-C2 → P1-C3）

- 清层不下楼死锁：45% TWall 局 → **1/500 局**（根除）
- TIMEOUT_WALL 16.8% → **7.8%**（-9pp）；节奏 x7
- 深层率 14.2%→8.2%（真实代价：节奏快→资源积累少→围殴死；但 P1-C2 的 deep
  近半是"TWall 耗尽"局，非胜利路径）
- 瓶颈清晰化：F1-F2 围殴 90.2% 是 P1-C4 主攻方向（战斗效率，非导航）
- 验证：60/60 ctest · Validator · 双跑 MD5 · 无注入对照 MD5 与 P1-C2 逐字节一致

---

# v1.4.3 — P1-C2 SimAI 毒对策 (2026-09-07)

> 决策层两条规则，零数值改动。DOT 是基线最大单一死因（33.6%），AI 此前对自身中毒状态全盲。

## 改动 (sim_ai.cpp)

- 中毒时药水线 0.35→0.55：毒 tick 3-6/0.5s，35% 线才喝必然被追上（喝 30HP 同时毒继续吃血）
- 自身中毒时攻击毒源怪 +0.25 分（orc/elite_orc/poison_wyrm，读 `on_hit_triggers` 数据）
- 感知源纯只读：`Player::active_buffs` + `Monster::on_hit_triggers`，无新耦合

## 500 局对比（P1-B → C1 → C2 累计）

- F1 死亡 96.6%→**82.4%**；深层率 3.2%→**14.2%**（×4.4）；最深 F11→**F14**
- 武器获取局 ×6（18%）；Boss 击杀局 ×22（44 局）；picks/kills 均 ×5.3
- DEATH_DOT 绝对数持平但结构改善：F1 死亡 -5.8pp 精确转化为深层 +5.8pp
- TIMEOUT_WALL 16.8% 成最大增长项 = P1-C3（拾取/搜刮效率）的接力信号
- 验证：60/60 ctest · Validator · 双跑 MD5 一致 · 详见
  `docs/P1C2_POISON_COUNTERPLAY_DATA_REVIEW.md`

---

# v1.4.2 — P1-C1 F1 教学层数值带下调 (2026-09-07)

> P1-B 基线判定"0% 通关是 F1 平衡数值问题"。本批仅动 2 处配置级数值，零逻辑改动。

## 数值改动 (floor_config.cpp + game_scene.cpp)

- F1 怪物数量 4→3，archer 权重 8→2（远程骚扰对走位差 AI 不公平压力）
- 初始治疗药水 2 瓶→3 瓶（毒 DOT 单次 24HP，2×30HP 不够对冲 2-3 次中毒）
- 不动：F2+ 全部楼层、growth_curve、怪物基础数值、毒 buff 强度

## 500 局对比（同 5 seeds × 100，`reports/p1c1/`）

- F1 死亡率 96.6%→**88.2%**；深层局（≥F3）3.2%→**8.4%**（×2.6）
- Boss 击杀局 2→**20**（max 双 Boss）；TIMEOUT_WALL 2.6%→9.8%（活到兜底的副产品）
- picks 0.49→1.39/局；DEATH_DOT 33.6% 持平（毒源未动，属 P1-C2）
- 判定：拐点已过但未达 20-30% 目标带；剩余瓶颈应交给 AI 行为修复（C2 毒对策/
  C3 拾取主动性）而非继续压数值（教学层 3 只怪已近下限）
- 验证：60/60 ctest · Validator 0 错 · 双跑 MD5 一致 · 详见
  `docs/P1C1_F1_TUNING_DATA_REVIEW.md`

---

# v1.4.1 — P1-B 500 局基线 + NPC 对话越界崩溃修复 (2026-09-07)

> P1-A4 三死锁修复后的正式 500 局基线采集；跑批过程发现并修复一处真玩家可触发的越界崩溃。

## P1-B-fix — NPC 二次对话越界崩溃（4/5 种子跑批必现）

- **表象**：`--sim 100 --sim-seed 3` 在 run 4 楼层切换后 SIGSEGV（exit 0xC0000005），4/5 种子在前几局崩溃
- **诊断**：gdb 栈 `strlen ← GameSceneInteraction::start_dialogue ← GameScene::_input`
- **根因**：`start_dialogue` 用 `i<5` 统一遍历对话池，但 `repeat_dialogue` 只有 2 槽且全满（无 nullptr 终止）→ NPC 重谈（`met=true`）越界读 3 个野指针
- **暴露链**：P1-A4 让 sim 能自动推进对话 → `met=true` 残留 → 下局重谈走 repeat 池 → 越界。**真玩家任何 NPC 二次交谈同样触发**（与 sim 无关）
- **修复 A**：`std::size` 按池真实容量遍历
- **修复 B**：`spawn_floor_npcs` 落位写错槽（`_npc_count-1` 是"最后创建"槽而非本 NPC 槽）→ 按 npc_id 定位
- **修复 C**：`new_game()` 不清 NPC 状态（`_npc_state/_npc_count/_dialogue` 跨局残留）→ 统一重置
- **修复 D**：BalanceReport `avg_*` 均值 int 截断（"1 局 2046 伤害 + 99 局 0" 算出 avg=0）+ `avg_heal` 漏序列化 → 改 float

## P1-B — 500 局正式基线（5 seeds × 100，`reports/p1b/`）

- 5×100 全 exit=0；同 seed 双跑 MD5 一致；60/60 ctest；Validator 0 错
- **胜率 0%**，96.6% 死于 F1：DEATH_MONSTER 61.4% / DEATH_DOT 35.6% / TIMEOUT_WALL 2.6%
- **攻击消费链复活但极窄**：picks 0→0.49/局，深层局（≥F3）0→16/500（3.2%），最深 F11（1 局双 Boss 击杀，55 杀 5383 伤害）
- 拿到武器的 15 局全部进入 F3+（fist_basic avg_floor=1.0 vs 真武器 2.5-8.0）——"活得久→捡到武器→活更久"正循环被 F1 数值掐断
- **结论**：M4 时 0% 是"AI 坏了"；P1-B 后 0% 是"F1 平衡数值问题"——16 个深层局证明链路走通后 AI 能推进。详见 `docs/P1B_BASELINE_DATA_REVIEW.md`
- **v1.5.0 Release Gate 依据**：崩溃修复必须发版；F1 数值带下调（P1-C1）建议在 Release 前完成

---



# v1.3.2 — G10 视觉链闭环 + P0 sim 死锁修复 + F15 平衡 (2026-09-01)

> G10.3→G10.7 视觉垂直切片全线贯通：游戏内空间可信度 + 游戏身份层。
> 期间穿插 P0 运行时死锁调查（sim 永久卡 F1）与 F15 镜像 Boss 平衡修复。

## P0-M1/M2 — Sim 永久 F1 死锁 (`0e3023c`)

- **初始误判**：疑似堆损坏（0xC5 = 诊断环境缺 MinGW DLL 的烟雾弹）
- **真根因**：v1.2.3 房间边界 × Encounter LOCKED 封门 × 旧传送落点不限房间 → 怪永久 IDLE + 玩家够不着 → 900s 死循环
- **修复**：`sim_ai_teleport_target()` 房间归属契约（分层落点/排除墙锁占用）+ 卡死检测锚点半径化 + 3 契约回归测试
- **红线达成**：同 seed sim 逐字节可重复（MD5 一致）、零 900s 死锁；SEH 崩溃处理器升级为符号化栈跟踪

## G10.4 — 地图视觉辨识度 (`c1026d0` · `018ef47`)

- 群系 tint 接线（监狱棕/火山橙红/虚空紫，色相主导替代亮度递减）
- 确定性 (x,y) 哈希变体 6% 污渍/4% 亮石块；墙底受光高光
- A.1 校色：floor.png 中性灰载体 + palette 亮度带统一 78-87

## G10.4-B — 战斗反馈 (`7be4700`)

- 命中 HitStop 分级（KILL>CRIT>HEAVY>LIGHT 单次不叠加）+ 命中音回归武器路径
- 暴击数字 alpha 溢出 bug 修复（0.6f 硬编码除数 → max_lifetime 字段）+ 1.6× 字号接线
- SWORD stage2 终结感（flash 0.18s/爆炸火花 12）

## G10.5 — 战斗空间统一 (`ed4d00d`)

- **AttackGeometry SSOT**：判定与 VFX 共用同一几何（origin/shape/range/width）
- 长矛 224px 突刺轨迹、双节棍 128px 范围环、短剑视觉收正、弓弩脚底冗余特效删除
- 斩弧 30° 斜置修正、矩形判定 +16→14、特殊段每击命中 VFX

## G10.6 — 玩家环境真实感 (`759f872`)

- C1 朝墙姿态衰减（前倾×0.3/重击 1.08/武器锚点减半）——攻击照常、仅视觉入墙收敛
- C2 停靠贴合（二分逼近最大合法位移，判定函数零改动）——停靠抖动 [0,3.33px)→稳定

## G10.7 — 游戏身份层 (B1-B4)

- **B1 图标管线** (`35a5ff8`)：剑与门 256 图标（16×16 逻辑像素+游戏色板）→ 六尺寸 .ico/.rc/CMake WIN32 GUI 子系统/SetWindowIcon/README logo；GUI 子系统 stdout 保活（dup2）
- **竞技场专属 BGM** (`eb781be`)：challenge 曲 160bpm C 小调急促波次战斗风
- **B2 舞台层** (`15a10ce`)：纵深渐变/透视地板/两侧石墙/拱门红光/火把余烬/径向 vignette
- **B3 角色层 v2** (`4c5dc7c`)：全画布 20 素材海报散布（左右纵深队列+顶部剪影带+暗影骑士出血裁切+底部战利品带），中央菜单净空
- **B4**：全屏菜单项点击修复（_activate 补 fullscreen 分支）

## F15 镜像 Boss 平衡三部曲 (`def9614` · `24e61ae` · `8bc602e`)

- 射程钳制：Echo 近战 ≤96px（禁复制长枪 224px）、弩 ≤220px
- Echo 时停窗口内自身伤害减半；玩家时停可见性（三重紫环+音效+常驻施法环）
- **数值护栏**（game.log 实锤 atk=11119 单击 3883）：HP ≤2000、ATK ≤玩家HP/8——威胁模型 6-8 击致死

## 验证

- **60/60 ctest 全绿**（+p0_teleport_test 3 契约）
- 同 seed sim 逐字节可重复；桌面版全程同步

---

# v1.3.1 — G9 审计闭环 + Asset Manifest + 字体迁移 + 竞技场/音效修复 (2026-08-30)

> v1.3.0 之后的技术债清理与基础设施升级批次，涵盖 G9 生命周期审计、
> 资源管理管线重构、中文字体替换、竞技场战斗修复与外部音效加载。

## G9.2–G9.4 技术审计闭环 (`b3143ec`)

- **G9.2 生命周期残留修复** — 清理 Monster/Player 析构顺序问题，UAF 防护补全
- **G9.3 RNG 边界隔离** — `CountingRng` 边界检查 + 确定性回归验证
- **G9.4 DoorState Truth Table** — 门状态转移表形式化验证，覆盖 4×4 状态组合

## G10.2-B1 Asset Manifest 基础 (`8f24e58`)

- `resources/sprites.json` schema v2：新增 `"assets"` 段，声明外部资源路径/来源/回退
- `ResourceManager::asset_by_id()` ID 查询 API + `load_assets_config()` 懒加载
- `world_validator.py` 新增 assets 段校验规则

## G10.2-B2 Door 迁移 (`683cee0`)

- DoorRenderer 4 种贴图从硬编码路径迁移至 Asset Manifest（`door.open/closed/locked/sealed`）
- 消除 4 处硬编码 `.png` 路径

## G10.2-B3A Audio Manifest + WAV 转换 (`9b8bd91` · `6e6c6a4` · `eaadd9a` · `8a559a5`)

- `sprites.json` 新增 `"audio"` 段：`timestop` / `domain_expand` 声明外部音频路径与回退
- **根因修复**：`InitAudioDevice()` 原在 `InitWindow()` 之前调用，导致 `LoadSound()` 对外部文件静默失败 → 移入 `SceneTree` 构造函数 `InitWindow()` 之后
- **MP3→WAV 转换**：`jojo_timestop.mp3` → `.wav`（44100Hz stereo 3.55s），`domain_expand.mp3` → `.wav`（5.47s）
- **ResourceManager 时序修复**：`AudioServer::init()` 在 `ResourceManager` 加载 `sprites.json` 之前运行，`asset_by_id()` 返回 nullptr → 改为硬编码相对路径直接加载，WAV 优先 MP3 回退
- 加载链：`LoadFileData()` → `LoadWaveFromMemory()` → `LoadSoundFromWave()`（绕过 `LoadSound()` 文件 I/O 问题）

## G10.2-B3B 字体迁移 (`3a902b1`)

- 替换系统字体为 `assets/fonts/NotoSansCJKsc-Regular.otf`（思源黑体，OFL-1.1 许可）
- 1831 码位（1729 CJK + 102 符号），per-codepoint 逐字验证 100% 覆盖
- `ResourceManager::_init_font()` 简化为单一字体路径，零系统字体依赖
- `font_codepoints.h` 移除 4 个不支持符号（☠⚔✗❄ + BOM），`tools/extract_chars.py` 同步更新
- `resources/landmarks.json` 等 JSON 中 ⚒→锤、⚔→剑 符号替换

## 挑战房怪物多样化 (`c6b4b02`)

- 新增 `_pick_monster_type(floor, wave, rng)` 按楼层/生态群落/波次分池：
  - 地牢 F1-5：slime/skeleton → orc/shadow → elite/charger/summoner
  - 熔岩 F6-10：fire_imp/bomber → orc/shaman → storm/golem/necro
  - 虚空 F11-15：shadow/void/dark_mage → ice_warden/priest → guardian/sentinel

## 竞技场修复 (`afb002e` · `0359b7f`)

- **时停修复**：竞技场 `m->update_ai()` 未检查 `time_stop_remaining` → 移除重复 AI 调用，由 `_player_ctrl.tick()` 内部门控统一管理
- **WeaponExecutor 补全**：竞技场新增 `tick_specials()` + `tick_projectiles()`（弩箭/矛/双节棍特殊攻击状态不再卡死）
- **进化名乱码修复**：`evo_name()` 返回 `const char*` 但内部 `get_evolution_text()` 返回临时 `std::string`，`.c_str()` 为悬垂指针（use-after-free）→ 改为返回 `std::string`
- **地面掉落修复**：`_draw_ground_items()` 在竞技场被 `WorldMode::CHALLENGE_ARENA` 门控跳过 → 开启渲染 + `enter/exit_challenge_arena()` 保存恢复 `ground_items` 防止坐标污染

## 验证

- **59/59 ctest 全绿**（新增 font_manifest_test 等资产管线测试）
- 音效验证：时停/领域展开 WAV 在 Release 构建下正常播放
- 竞技场验证：地面掉落可见可拾取、退出后地牢物品恢复

---

# v1.3.0 — Batch 3: 经济系统 + 赌徒房 + 挑战房 (2026-08-29)

> v1.2.x 门/房间遭遇系统之上的内容扩展批次（3A→3I），引入金币经济与两类可重复特殊房。
> 设计: `docs/BATCH_3A_ECONOMY_REWARD_DESIGN.md` · `BATCH_3B_GAMBLE_ROOM_PLAN.md` · `BATCH_3E_CHALLENGE_ROOM_FINAL_PLAN.md` · `BATCH_3I_CHALLENGE_PORTAL_PLAN.md`

## Batch 3A — 经济与持久化基础

- `PersistenceScope(FLOOR/RUN)` 圣物作用域；Player 金币/钥匙字段
- `get_sell_value` / `sell_item`（装备出售）；RewardManager 统一奖励发放
- **Save v4**（跨版本兼容追加字段）；HUD 金币/钥匙显示；5 测试

## Batch 3B — 赌徒房 MVP

- 金币开房（40 + floor×10）；奖励池 75% 装备 / 20% 钥匙 / 5% RUN 圣物
- 奖励耗尽回退钥匙；`is_repeatable` 旁路特殊房一次性限制；8 测试

## Batch 3C — 背包出售 UI

- `[T]` 出售选中装备（sell_selected_item 静态 helper）+ 键位提示更新；8 测试

## Batch 3D/3E — 挑战房审计与设计冻结

- SpecialRoom/Key/Room/Monster/Reward 全系统架构审查
- 设计冻结: 7 阶段状态机 + ChallengeRoomController + 3 波×4 怪 + 确定性 RNG 派生 + 背包满奖励 fallback

## Batch 3F/3G — 挑战房 MVP + HUD

- ChallengeRoomController: 7-phase 状态机、出口附近放置、波次战斗、HUD wave 显示（3F，14 测试）
- 挑战房 HUD: 进度条 + 击杀计数 + 剩余波次 + floor 横幅 + Boss 变体（精英/双怪）（3G，10 测试）

## Batch 3H/3I — 传送门系统 + 竞技场修复

- **传送门系统**: 出口传送门生成 → E 键交互 → 选择面板 → 独立竞技场地图 → 返回传送门
- **Arena 移动式架构**: 进场保存地牢怪物/地图状态，退场恢复
- 竞技场内战斗修复: 武器 recovery_timer/冷却正常推进；VFX/连招/演出 tick（特效与屏震正确过期）
- E 键返回直查 `challenge return_portal_tx/ty`（竞技场无 special_rooms）；ESC 退竞技场跳过存档但允许回标题
- 性能: O(n²) 怪物列表构建移出内层循环；字体扩容（1835 codepoints）
- `challenge_portal_test` + `challenge_room_test` 合计 53/53 全绿

## 期中热修复

- 字体 codepoints 修正 + 赌徒房奖励洗牌修复 + 背包金币显示（`3a9c13b`）
- 赌徒房生成侧 + 全屏缩放 + 特殊房放置（`a6ed40a`，Batch 3G）

## 验证

- **53/53 ctest 全绿**（新增 economy/challenge_room_test、challenge_portal_test 等 36 测试）
- GitHub Actions CI 通过；README 结构/键位/存档 v4 同步更新
- 实机验证: 传送门进出竞技场、波次战斗、返回恢复地牢状态

---


# v1.2.6 — Boss FOV + 弹幕穿墙 + UI 键位提示 (2026-08-28)

## Boss FOV 视野系统

- **Tile** 新增 `boss_visible` 字段
- **GameMap::update_boss_fov()** — 360°射线投射，Boss 周围8格视野，受墙壁遮挡
- **主地图**：Boss 视野区域叠加红色半透明覆盖（已探索+boss可见+不在玩家视野）
- **小地图**：Boss 视野绘制 — 未探索区域暗红色，已探索区域红色叠加
- 每帧追踪 Boss 实际位置并更新 FOV

## 弹幕墙体碰撞

- `Projectile` 新增 `pierce_walls` 字段（默认 false）
- 玩家/怪物弹幕 tick 循环加 `is_walkable` 墙体检测，碰墙销毁
- 弩箭 power shot（stage 3）`pierce_walls = true` 可穿墙
- Boss BarrageSkill 已有独立墙体碰撞（Shot struct）

## UI 键位提示

- 战斗 HUD 右下角：`[R]圣物 [B]背包 [F1]日志 [M]地图 [ESC]保存`
- 标题画面操作说明：新增 `M - 小地图`
- 小地图面板下方：`[M] Map` 提示

## 测试

- 44/44 ctest 全绿

---

# v1.2.5 — Door Visual System: Kenney 素材 + 状态动画 (2026-08-28)

> 4种门状态4种外观 + 0.3秒过渡动画

## DoorRenderer (新增 `src/game/rendering/door_renderer.h/.cpp`)

- **素材**: Kenney Tiny Dungeon 16×16 tile (tile_0003/0018/0022) → 32×32
- **OPEN**: 空拱门 `tile_0003` — 暗色拱形通道
- **CLOSED**: 木门 `tile_0022` — 棕色木门+把手
- **LOCKED**: 木门 `tile_0022` + 红色锁 icon (代码绘制)
- **SEALED**: 深色门 `tile_0018` + 紫色封印十字纹 (sinf 脉冲)
- **动画**: 状态切换触发 0.3s 过渡 — 旧 tile alpha 渐隐 + 新 tile alpha 渐显 + scale 0.8→1.0
- 叠加标记只在最终状态绘制，过渡中不画

## 集成

- `GameMap::set_door_state()` 自动触发 `DoorRenderer::on_state_change()`
- `GameMap::draw()` DOOR 分支 → `DoorRenderer::inst().draw_door()`
- `GameScene::enter_floor()` 初始化, `_process()` 每帧 update

## 测试

- `door_renderer_test`: 7 case — 单例/init安全/绘制不崩溃/动画状态切换/过渡完成
- 44/44 ctest 全绿

---

# v1.2.4 — Batch A: 门交互 & 碰撞修复 (2026-08-28)

> 分离视觉/碰撞尺寸 · E 键开门 · LOCKED 门语义 · SimAI 门处理

## Entity 碰撞/视觉分离 (A1)

- `Entity` 新增 `collision_size` 字段 (28×28)，独立于 `size` (32×32)
- `sync_rect()` 将碰撞矩形居中放置于视觉矩形内
- `draw_rect()` 仍返回视觉尺寸 (32×32)，攻击判定用 `rect` (28×28)
- Player 构造: `entity(x, y, 32, 32, 28, 28)`

## 锁门 API (A1)

- `GameMap::lock_room_doors()` — 设置 DoorState::LOCKED (不可 E 键打开)
- `blocks_sight()` 改为 `door_state != DoorState::OPEN` (LOCKED 也遮挡视线)
- `close_room_doors()` → CLOSED, `lock_room_doors()` → LOCKED, `open_room_doors()` → OPEN 严格对称

## E 键开门 (A2)

- PlayerController 拾取处理中新增 E 键 → 检测4方向 CLOSED 门 → 打开
- 仅响应 CLOSED 门 (LOCKED/SEALED 不可 E 键打开)

## 删除自动撞门 (A3)

- `_update_player()` 中 `try_open_door_toward()` 调用已移除
- 现在必须显式按 E 键开门

## Room Encounter → LOCKED (A4)

- `RoomManager::_try_lock()` 改调 `lock_room_doors()` (原 `close_room_doors()`)
- 怪物房间封锁用 LOCKED 语义，防止 E 键误开

## SimAI 门处理 (A5)

- `_tile_rect_walkable`: CLOSED=true (可穿透), LOCKED=false (绕路)
- `best_action()`: 目标 tile 为 CLOSED 门 → 返回 "pickup" (先开门)
- LOCKED 门对 Sim 不可规划

## 硬编码修正 (A6)

- 8 处 `+16` 改为 `rect.width / 2` / `rect.height / 2` (game_scene.cpp ×6, sim_ai.cpp ×2)
- 测试 `place_player()` 修正碰撞中心偏移 (room_encounter_test T3)

## 新增测试 (A7)

- `entity_center_test`: 视觉中心=碰撞中心不变量、sync_rect保中心、Player碰撞尺寸、draw_rect用视觉尺寸

## 验证

- 43/43 ctest 全绿; 构建 0 警告; sim smoke 通过

---

# v1.2.3 — 怪物房间边界约束 + 小地图位置修正 (2026-08-28)

## 怪物房间约束

- **AI 视觉**：`_decide_state()` 增加 `room_at()` 检查 — 玩家不在怪物房间时强制 IDLE，禁止 CHASE/ATTACK
- **AI 移动**：`_apply_movement()` 目标 tile 不在怪物房间内 → 阻止移动
- **远程技能**：SNIPER/RAPID_SHOT/SCATTER/CONTROLLER 跨房间 → 跳过攻击
- **传送技能**：AMBUSH/CHARGE/LEAP 目标不在房间内 → 跳过
- **脱困传送**：`_unstuck_wedged_monsters` 只传送到怪物自己房间内，无有效 tile 则重置到 home
- **架构**：`MonsterAI::update()` 新增 `monster_room`/`player_room`/`room_mgr` 参数，由 `GameScene::_update_monsters()` 计算传入

## 小地图位置

- 面板上移 26px (`sh - MINIMAP_HEIGHT - 14` → `sh - MINIMAP_HEIGHT - 40`)，不再遮挡底部快捷键提示 `[R]圣物 [B]背包 [F1]日志 [ESC]保存`

## 验证

- 42/42 ctest 全绿; 构建 0 警告; 桌面已同步

---

# v1.2.2 — Batch 2C: Room Encounter (进房→封门→清房→开门) (2026-08-28)

> 在 A1 密封拓扑 + DoorState + R1 接触开门之上, 实现以撒式房间战斗状态机。
> 设计: `docs/BATCH2C_ROOM_ENCOUNTER_DESIGN.md` (已审核)

## RoomManager (新增 `src/game/world/room_manager.h/.cpp`)

- **状态机**: IDLE → ARMED → LOCKED → CLEARED
  - 玩家进入有怪房 → ARMED → (E1 无压门 / E2 房怪在房内 / E3 原子关门) → LOCKED 全门 CLOSED
  - 房内怪清零 → CLEARED 门 OPEN
- **性能约束** (用户审核): 只维护/检查当前激活 Encounter, IDLE 房间零扫描; 玩家跨 tile 才检测
- **映射固化**: build() 时一次性建立 房间矩形 + 门组, 运行时不搜索门
- **解耦**: 通过回调 (on_locked/on_cleared) 通知 GameScene — 可单元测试

## Door Group API (GameMap)

- `close_room_doors(door_tiles)` / `open_room_doors(door_tiles)`: 多门房间原子开闭 (E3)

## EventBus

- +`ROOM_LOCKED` / `ROOM_CLEAR` (2 枚举)

## 边界规则

- E1 实体压门 → 暂缓落锁 | E2 房怪门外 → 暂缓 | E3 多门原子
- E4 Boss 房跳过 (现有 BOSS_INTRO 流程) | E5 特殊房照常 | E7 楼梯房照常
- 击退/传送推出 LOCKED: CLOSED 门=碰撞墙 (Batch 1 语义天然防)

## 验证

- **42/42 ctest 全绿** (新增 `room_encounter_test` 6 用例: 闭环/无怪不触发/压门不锁/Boss 跳过/多门原子/映射集成)
- 构建 0 警告; 确定性保持 (同 seed 逐字节一致)
- Sim 回归: seed100 100 局 F5=39% (Room Encounter 引入真实关门, Sim 经 S1 正常通过, 无卡死)
- 清房掉落钩子 Batch 3 接; 封门演出仅一次性 room_msg

---

# v1.2.1 — Batch 2B: Door Interaction (R1 接触开门 + S1 Sim 语义) (2026-08-28)


> Batch 1 (v1.2.0) 完成 DoorState 数据模型后, Batch 2B 接入交互层。门保持默认 OPEN (D2 决策), 不改变 gameplay。

## R1 — 接触开门

- `GameMap::try_open_door_toward(rect, mx, my)`: 玩家移动中心 tile 指向 CLOSED 门时自动开启 (无按键)
- 接入 `PlayerController` 移动碰撞: 被 CLOSED 门阻挡时先开门再移动 (水平/垂直两轴)
- `GameScene::on_door_opened()`: 开门后立即重算 FOV (门后区域揭示)

## S1 — Sim 语义

- `_tile_rect_walkable`: CLOSED 门视为可通行 (Sim 与玩家共用 R1 规则, 零 Sim 专用逻辑)

## 验证

- **41/41 ctest 全绿** (新增 `door_interact_test` 5 用例: 四方向接触开门/非门不触发/CLOSED 挡人挡视线/生成图默认 OPEN)
- 构建 0 警告; 确定性保持 (同 seed 逐字节一致)
- Sim 冒烟: seed100 50 局与 Batch 1 基线一致 → **未改变 gameplay** (门默认 OPEN)
- 门 CLOSED 语义已由 Batch 1 `door_seal_test` 覆盖; 真实关门逻辑 (Room Encounter) 在 Batch 2C

---

# v1.2.0 — 地牢密封 (Batch 1): 门是房间唯一孔径 + DoorState (2026-08-28)

> A1 孔径修复把地牢从"开放地板团块"修成真正的 Room→Door→Corridor 拓扑。
> 详细: `docs/BATCH1_DUNGEON_SEAL_ACCEPTANCE.md` / `docs/BATCH1_DUNGEON_SEAL_IMPL_PLAN.md`

## A1 — Door Aperture Integrity（孔径完整性）

- **问题**（审计发现）：`_carve_diamond` 雕走廊时在房间环墙留下平均 6.25 个/房的非门缺口（"隐形门"），关门无法密封、FOV 隔门泄露 93.2%
- **修复**：`_repair_room_apertures`（确定性后处理，零 RNG）— 环墙缺口回墙(94%)/door 化(6%)
- **结果**（27 seeds）：密封率 0%→100%，非门缺口 6.25→0，房间内部泄漏 0，无死房，全图连通，门数 18.7→22.7
- **INVARIANT(seal)**：`door_seal_test` T1-T6 永久回归（27 seeds = 7 基准 + 20 fuzz）

## DoorState 数据模型

- `Tile.door_state`（OPEN/CLOSED，LOCKED/SEALED 预留）+ `GameMap` 门态 API
- 语义：OPEN = walkable + 透视线（现状保持）；CLOSED = 不可走 + 挡视线（Batch 2 启用）
- 生成后门默认 **OPEN**（D2 决策：独立验证孔径修复与 FOV 效果）
- FOV 半径可配置：`FOV_RADIUS_DEFAULT=8` + `FloorConfig.fov_radius`（0=默认）

## 验证

- **40/40 ctest 全绿**（含 door_seal_test 6 子断言 + fov_test 3 新用例）
- 构建 0 警告；确定性保持（同 seed 逐字节一致，A1 零 RNG 消耗）
- Sim 回归：baseline 10.4% → 2.6%（A1 后新基线）。归因审计排除 path/chokepoint/walkable/怪物出生/Boss 结构后判定为**确定性 Sim 的决策链分叉**，非拓扑 bug。**用户裁决接受新基线**，真人 F5 体验并行验证中

---

# v1.1.0 — 可见性与空间体验 (Phase 1-3): FOV + 地牢拓扑 + Minimap (2026-08-28)


> v1.0.0 之后的地牢空间感三连深耕：从"做完"到"做得像"。全部保证 FOV/Save/AI/战斗系统零改动（除 Phase 1 自身）。

## Phase 1 — FOV 可见性系统

- **Tile 三层状态**：`is_visible`（当前帧 FOV 内）/ `is_explored`（曾探索）/ `is_walkable`（碰撞，独立于可见性）三字段解耦
- **360° 射线投射**：`update_fov(cx,cy,radius)` 逐度投射，撞墙/越界即断；`reset_visibility()` 进层清空
- **三层渲染**：未探索=全黑不渲染；当前可见=全亮；已探索但不可见=60% 暗（记忆态）
- **实体剔除**：怪物/NPC/地面物品按中心 tile 的 `is_visible` 剔除，未探索区看不到生物
- 8 个 FOV 单测（`tests/world/fov_test.cpp`）
- 阻断问题：实体可见性判定错误、贯穿墙视线

## Phase 2 — 地牢拓扑 (Room → Door → Corridor)

- **`TileType::DOOR`**：`walkable=true`、`blocks_sight=false`（静态开启门，预留 CLOSED/LOCKED/SEALED 扩展）
- **边缘连接算法**：`_pick_room_edge`（房间边缘中点）+ `_compute_door_pos`（向外 1 格放门）+ `CorridorConnection` 结构
- **`_carve_diamond` 墙壁保护**：`if (g[ty][tx]=='#')` 只雕刻墙壁，走廊绝不侵入房间 Interior
- **门边界安全**：`_pick_room_edge` 过滤 Door 越界的边缘（地图侧)
- 12 拓扑测试 + 5 结构回归（`dungeon_topology_test` / `dungeon_verify_test`，永久保留）
- 验收报告：`docs/PHASE2_ACCEPTANCE_REPORT.md`（房间环墙覆盖率 71~78%，墙体密度 41~52%，无巨型开放区）

## Phase 3 — Minimap 小地图

- **`MinimapRenderer`**（`src/game/ui/`）：只读 `isExplored/isVisible`，**无第二套探索状态**；职责=坐标换算/绘制/标记/面板背景
- **不泄露原则**（纯函数可测）：`should_show_boss`（最后已知位置，仅已探索）/ `should_show_stairs`（发现后永久）/ `should_show_entity`（仅当前可见，离开视野即消失）
- **常量集中**：`MINIMAP_TILE_SIZE/WIDTH/HEIGHT`（非魔法数字，便于扩展）
- 右下角常驻面板，**M 键**开关（默认显示）；Boss 不实时追踪不可见区移动
- 12 单测（`tests/ui/minimap_test.cpp`）

## 验证

- **39/39 ctest 全绿**（含 minimap 12 + dungeon_verify 5 + dungeon_topology 12）
- world_validator 0 error 0 warning
- 多 seed 实机 smoke test 无崩溃；桌面打包版已同步
- 完整审计/设计/验收：`docs/PHASE1_FOV_PLAN.md` / `PHASE3_MINIMAP_PLAN.md` / `PHASE2_ACCEPTANCE_REPORT.md`

---

# v0.9.35 — 实测反馈修复: 背包键位冲突 + 怪物房间守卫 + 通关专属 BGM (2026-08-25)

## 玩家实测三连 (来源: 试玩反馈)

- **背包 D 键二义性**: 打开背包后按 D 丢弃被翻页抢占 (D 同时绑定 move_right, 翻页判断在丢弃之前) — 重构背包分支: X/U/D 动作键优先判定, 光标移动改用 WS/↑↓, 翻页改用 ←→ 方向键, 彻底解耦动作与导航
- **怪物房间守卫 (leash)**: 原 IDLE 随机巡逻使怪走出房间 → 进入视野全图追击 → 前期怪涌向主角、中后期无怪可打。新增出生锚点 + 双重束缚: 巡逻半径 4.5 格 (超出折返) / 追击上限 8 格 (超出放弃回家); 掉血即视为挑衅解除束缚 (含毒/环境伤); Boss 不受束缚
- **通关专属欢快 BGM**: 新增 victory 曲目 (C 大调 I-V-vi-IV 进行 @132bpm, square 主音上行琶音) — 原通关动画沿用紧张 Boss 曲直到回标题; 经 VictoryScene::get_bgm_name() 声明走 change_scene 场景级管线自动切换, 回标题后由 TitleScene 的 title 曲接管

## 验证

编译 0 警告; 34/34 ctest; **300 局 sim 回归 9.0%** (区间 6-10%, leash 后 sim 由玩家 BFS 主动寻怪, 胜率稳定)

---

# v0.9.34 — AI 系统代码审查修复: 11 处算法正确性 bug (2026-08-25)

> 源起: 全仓 AI 子系统源码级深度审查 (发现与修复记录整理于 docs/AI_LEARNING_GUIDE.md), 修复其中经回归验证的 11 处

## BTAgent (行为树, `--sim-ai bt`)
- **P0-1 接线修复**: confirm/descend 动作已创建但从未挂入树 (根节点 push 的是裸 Condition, Selector 命中即短路) — 改为 Sequence{cond, act}, BT agent 首次具备下楼/确认能力
- **P0-2 时间语义**: 技能冷却判断 can_use(0) → can_use(_game_time); 新增 BTAgent::set_time 注入链 (game_scene 每帧同步, 原 BT 模式拿不到时间 → 用过一次技能后永久假阴性)

## Q-Learning (`src/ai/rl/`)
- **B1 终局自举污染**: update() 增加 done 参数 — done 时 target=reward 不自举 max Q(s') (原把"键不存在"当终止, 真终局反而自举); rl_runner 两处调用传 env.is_done()
- **B2 学习率衰减**: α/(1+0.05·visits(s,a)) — 常数学习率违反 Σα²<∞ 收敛条件, Q 值永远震荡
- **B3 击杀奖励增量式**: 原"+50/尸体/步"每步重复发放 (prev_alive 死变量佐证原意), 改为 prev vs now 差值一次性 +50

## MirrorAgent Thompson 采样 (`src/ai/mirror/`)
- **MP1 先验爆炸**: init_prior 的 +2 伪计数随存档每局固化叠加 + import 纯加法无遗忘 → 后验无界增长, 探索概率随局数衰减至零; 双重修复: ① export 扣除本局 pending 先验 (画像只服务当局冷启动) ② update 引入全臂折扣遗忘 λ=0.995 + floor 0.25 (非平稳环境恢复探索)

## MCTS (`src/ai/mcts/`, `--sim-ai mcts`)
- **A1 奖励归一化**: sigmoid(score/250) 映射 [0,1] — C=√2 的理论前提是单位化奖励, ±1000 量纲下探索项上界 ~2.5 永远翻不动利用项 → UCT 退化为纯贪心
- **A2 WAIT 偏差**: expand-all 后恒取 children.back()(WAIT) 使新节点首轮统计系统性偏向等待 → 按迭代序轮换 (保持确定性)
- **A3 回传折扣**: 删除 0.95 衰减 — 不同深度均值不可比而 UCT 在同一父下比较兄弟
- **A6 冷却伪造**: 快照 attack_cooldown 恒 0.5/skill 恒就绪 → 根节点永久禁用普攻; 改读真实 remaining_cooldown (build_sim_state 增加 game_time 参数)

## DecisionAgent (默认 sim AI)
- **P1-2 治疗优先级倒置**: 自愈槽遍历无 break, 最低优先级槽反向覆盖 → _skill_priority 首个可用即 break
- **P0-3 死区 (实测回退)**: 确认 [48px, ideal] 区间 attack/move 双零且"拉开距离"分支为不可达死代码; 尝试激活后 200 局胜率 10%→3.5% (风筝震荡破坏 Q3.12 平衡), **回退保留站桩行为**并在注释中记录缺陷与数据

## 验证
- 编译 0 警告; 34/34 ctest; world validator 0 error
- **500 局平衡回归 9.0% (45/500)** — 区间 6-10% 内 (基线 v0.9.33 为 10.0%, 波动范围内)

---

# v0.9.33 — 收官体检修复: 死配置清理 + 木桶闭环 + Boss 冷却恢复 (2026-08-19)

## 全面代码体检 (240+ 源文件)
- **删除 hazards.json 死链路**: 零消费者配置 (G6.3 未接入) — 删 JSON/hazard.h/cpp/加载/测试引用/world_validator 6 处校验; `_is_hazard_near` (熔岩/毒池/尖刺) 为活系统保留
- **EXPLOSIVE_BARREL 最小闭环**: 玩家攻击 (近战/武器) 或敌方投射物命中 → 点燃 (0.6s 引信红闪警告) → AOE 爆炸 (2 格, 3×arena_scale, 玩家+怪物) → 爆炸 VFX + 震屏 → 销毁; sim_ai 危险感知避开木桶; 复用现有 VFX/伤害系统, 无新 Manager
- **Boss 技能冷却恢复判定**: can_use 读端接入 (原写-only 死数据) — 连招命令 + 普攻循环技能释放前判冷却, 冷却中该步退普攻 (不空转)
- **其他**: 修复 bgm_engine 音符解析 narrowing 警告 (显式 char 转换)
- 验证: 34/34 测试, validator 0 error, 编译 0 警告; **200 局 sim 实测 38 次 点燃→爆炸 完全成对** (伤害随楼层缩放); 500 局平衡回归 **10.0%** (区间 6-10% 上沿, Boss 技能冷却后略升); F1-F15 全楼层 sim 跑通无回归
- 清理 2.8GB game.log (验证日志已重建为干净小文件)

---

# v0.9.32 — v1.0.0 Release Standard 验收 (五项 Stable 全部达标) (2026-08-19)

## 五项 Stable 冻结验收
- **Save Stable**: 新增 `SaveStable.*` 3 验收测试 (v1 旧档兼容/坏条目容错/全字段 roundtrip); **修复真实 bug** — elem 字段写元素名 ("fire") 而读端 atoi=0, 元素类型读档永久丢失, 改写 int (M4b-fix)
- **API Stable**: 对外契约冻结 2+ 版本 (存档 v3 格式/Registry MergeMode/Mod 管线/Replay hash 链)
- **Mod Stable**: mods scan + ModProvider + MergePatch + DependencyResolver 全链路 + registry 引用完整性测试
- **Regression Stable**: Q3.14 确定性对拍 (逐字节一致) + 500 局平衡回归 8.0% (区间 6-10%) + 37 gtest 全绿
- **Performance Stable**: sim 500 局并行 53s / 单核 ~9.4 局/s / 全量测试 0.46s
- 验收报告: `docs/V1_0_0_ACCEPTANCE.md`

---

# v0.9.31 — M4b: 地狱火魔领域作战 (弹幕演出 + 机制阶段 + Boss 房地形) (2026-08-19)

## M4b.1 弹幕图案化 (茶杯头式)
- `BarrageSkill` 图案化: `pattern` 0=扇形 1=环形 2=螺旋多波; `waves/wave_interval` 波次发射; `spiral_turn_deg` 每波偏转
- 弹丸飞行从硬编码 0.016f 步进改为帧间时间差 (修复帧率相关弹速)
- fire_demon 接入连招路径: probe/press/rage 三模板 (含 5 波螺旋弹幕), 数据驱动 (`BossSkillDef` 扩展)
- `BossEncounterController::phase()` 接线 `_select_combo`: OPENING/PRESSURE→probe, CONTROL→press, LAST_STAND→rage

## M4b.2 机制阶段激活 (MECHANIC_PHASE)
- 核心破坏 → 弹幕演出段 (Boss 无敌, 每 1s 强制快速弹幕风暴, 演出 4s) → 易伤窗口 (奖励节奏)
- 核心超时 → 直接易伤 (不变); 狂暴期演出减半; `domain_cycle_count` 双计数修复
- `domain_config.mechanic_duration` 数据驱动; 播报文案 + 冻结演出增强

## M4b.3 Boss 房机制地形 (熔岩环带安全区)
- `TileType::LAVA`: 可走地砖 + 橙红脉动绘制 + 0.5s 灼烧 (玩家/非 Boss 怪物, Boss 免疫)
- F10 Boss 房: 清空随机 ArenaObject + 中央安全区 + 外圈熔岩带 (欧式圆环, 自适应房间尺寸)
- `BossArenaDef.terrain` 数据驱动 (enabled/safe_radius/lava_band/clear_objects); `DungeonGenerator::get_boss_room_rect()`
- SimAI 危险视野感知熔岩 (3x3 邻格), BFS 可穿越

## 验证
- 500 局评估: 7.0% (s7 9% / s500 5% / s1000 6% / s2000 11% / s9999 4%) — 在 6-10% 目标区间, 较 RL 基线 6.6% 微升 (Boss 强化)
- 34/34 单元测试 + World Validator 通过

---

# v0.9.30 — RL 决策层接入镜像 Boss (F15 实战) (2026-08-18)

## RL 训练产物 → 运行时决策 (闭环打通)
- `QAgent::exploit_action(obs)`: 纯 exploit 决策 (无 SimulationState), 未见过的状态返回 -1 (不接管)
- MirrorAgent 仲裁链插入 RL 层: ML → 战术链 → **RL** → 克隆 → Thompson
- 镜像语义: Q 表学的是玩家视角最优策略 → 映射为 Boss 反制臂 (ATTACK→COMBO, SKILL→SKILL, MOVE→按距离 APPROACH/RETREAT)
- `MirrorBattleState → Observation` 适配 (字段与 rl_runner 训练场景对齐), 按玩家风格加载 `saves/rl_mirror_q_<STYLE>.json`
- 文件缺失 → 不注入 (降级现有仲裁链, 安全); 观察期 (phase<2) 不启用
- 验收统计: MirrorDebugSnap 新增 `rl_used` 计数, HUD 摘要仲裁[Clone/ML/RL/Tho]

## 验证
- 实测: 战斗仲裁 `[Clone:0 ML:0 RL:11/25/26 Tho:0]` — RL 完全接管仲裁, Thompson 不再触发
- 500 局评估: 胜率 8.6% → 6.6% (s7 6% / s500 6% / s1000 4% / s2000 7% / s9999 10%) — RL 镜像 Boss 变强, 仍在目标区间 6-10% 内
- 34/34 单元测试通过

---

# v0.9.29 — RL 训练收敛: epsilon 退火, 胜率突破 95% (2026-08-18)

## epsilon 退火
- `QAgent::set_epsilon()`, 训练循环按进度线性退火: 0.12 → 0.005 (常量 `EPS_START/EPS_END`)
- 原理: 固定探索率 0.12 是天花板 (~91% 封顶), 后期降探索后利用率提升, 胜率突破 95%
- 新增"末段 10% 低探索统计" (tail): 训练末尾 500 局 (epsilon≈0.005) 胜率即真实收敛水平

## 训练结果 (续训 5000 局/风格, 累计 ~20000+ 局)
| 目标 | 200局基线 | 退火前 | 退火后 tail (低探索) |
|------|-----------|--------|----------------------|
| RL TRAIN | 100% | 100% | **100%** |
| AGGRESSIVE | 74.5% | 91.5% | **96.8%** |
| DEFENSIVE | 80.0% | 91.2% | **99.0%** |
| SNIPER | 72.5% | 91.9% | **96.4%** |
| BALANCED | 64.5% | 90.7% | **99.2%** |
- 全部 ≥95% 达标; Q 表已饱和 (2380-2374 条目, 状态空间覆盖完毕)
- 34/34 单元测试通过

---

# v0.9.28 — RL 训练管线: 入口合并 + Q 表持久化续训 (2026-08-18)

## Q 表持久化
- `QAgent::save(path)` / `QAgent::load(path)`: JSON 格式 (`{"q": {obs|action: value}}`), 目录自动创建, 损坏/缺文件安全返回 false
- `--rl-train N`: 训练前自动加载 `saves/rl_qtable.json` (存在则继续训练), 训练后保存
- `--rl-mirror N`: 4 风格各独立 Q 表 `saves/rl_mirror_q_<STYLE>.json`, 同样支持续训
- 训练产物不纳入版本库 (gitignore 新增)

## 命令行入口合并
- 原 `--rl-train` 分支提前 `return 0` → `--rl-mirror` 永远不可达 (死路径)
- 改为顺序执行: `run_rl_mode` → `run_rl_mirror_mode` → 统一退出, 两参数可同跑

## 验证
- `--rl-train 100 --rl-mirror 50` 同跑正常, 第二次运行 `[load] ... entries — 继续训练` 生效
- 实测续训: 镜像 4 风格 200+50 局 (AGGRESSIVE 2078→2369 条目), 单风格胜率 48-86%
- 34/34 单元测试通过

---

# v0.9.27 — Sim 确定性修复: 指针键/跨层残留三连 (2026-08-18)

## 背景: 同种子双进程评估结果逐字节不一致 (可复现性回归)
- 症状: `--sim N --sim-seed S` 两次运行日志在运行中间帧分叉, 报告随机不同 (胜率 5%~15% 抖动)
- 排查: 对拍 (RNGDBG 打点 + rng.draws 轨迹) 缩小到 F5 f=4 帧内击杀分叉 — 状态全同却一只史莱姆死亡
- 根因定位: 三处裸指针跨进程不确定 (堆地址不同) + 跨层/跨局残留 (地址复用 → 污染新对象)

## 根因 #1: 怪物脱卡状态指针键
- `_unstuck_last_pos/_unstuck_since` 以 `const Monster*` 为键 — 换层后旧怪释放, 新怪 malloc 地址复用 → 残留键把新怪当成"卡住已久"秒传送
- 修复: 键改 `uint64_t instance_id` (monster.cpp 静态递增计数器), enter_floor 时清空两 map

## 根因 #2: SimAI 路径记忆指针键
- `_mem_target` 以 `const void*` 记录上一目标 — 同内存地址的新怪沿用旧路径记忆 → 决策分叉
- 修复: 改 `uint64_t` + 空指针判 `mem_t ? mem_t->instance_id : 0`

## 根因 #3: 双节棍连击自动追踪裸指针 (主凶)
- `WeaponSpecialState::tracked` 存 `Monster*`: 激活于 F1 (第3段连击), 跨 ~3700 帧残留到 F5 仍 active
- 换层后地址复用: 一个进程的 tracked 恰好指向史莱姆 (打死, hp 35→0), 另一进程指向别的怪 → 帧内击杀分叉 (该帧 rng 消耗 13 vs 6)
- 修复: 改 `uint64_t tracked_instance`, tick_specials 用 `std::find_if` 按 instance_id 查找 + is_alive 校验, re-acquire 时同步更新

## 验证
- 三种子 (500/1000/2000) × 20 局 × 2 批: 全部逐字节一致 (130万行级对拍)
- 评估基准 (修复后确定性): seed2000 5% / seed1000 15% / seed500 10%
- 大样本验证: 5 种子 × 100 局 = 500 局, 胜率 8.6% (43/500, seed7 10% / s500 9% / s1000 8% / s2000 9% / s9999 7%), 对比 Q3.12 基线 5.8% — 确定性修复后进入目标区间 6-10%; Boss 击杀 F5=63% F10=33% F15=11%
- 34/34 单元测试通过

---

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

# v0.9.26 — Q4 品质打磨批2: 反馈补全 (2026-08-11)

## Q4.7 玩家受击红屏
- `trigger_hit_flash()` + `hit_flash_timer`: 全屏主题 hit_flash_tint 叠加, alpha 随计时衰减
- 受击两处 (弹幕路径/近战路径) 同步触发 — 视觉反馈闭环

## Q4.6 VFX recipe 消费 sfx/camera_shake 字段
- `play_recipe` 现消费 recipe 的 `sfx`/`camera_shake` — 此前 28 处配置全部死数据
- 补 3 个缺失合成音: `ice_crack`/`lightning`/`summon`
- 经 ServiceLocator 间接访问 (VFXServer 值对象不持引用, 模块边界不变)

## Q4.5 UI 音效 + 标题菜单高亮
- 新增合成音: `ui_click` (短促)/`ui_confirm` (双音上行)
- 标题菜单: 鼠标悬停高亮 (禁灰项不可悬停) + hover 切换音效 + 左键点击激活
- `TitleScene::_activate()`: 键盘/鼠标共用动作分发 (单一职责)
- 游戏内面板开关 (背包/圣物/任务日志) 播放 ui_click

# v0.9.25 — Q4 品质打磨批1: 打击感与音频补全 (2026-08-11)

## Q4.1 HitStop 修复 (隐藏全局短板)
- `freeze_timer` 原只递减不消费 — 所有 trigger_freeze 调用形同虚设
- `PresentationSystemDirector::is_frozen()` + GameScene 主循环接入:
  冻结期跳过世界模拟 (怪物/弹幕/Buff/玩家), 仅表现层计时器推进
- 打击感三件套 (HitStop/震屏/飘字) 至此全部真正生效

## Q4.2 BGM 循环 + stop 修复
- `BGMEngine::stop()` 原停的是 `_cache.begin()` (第一首) 而非当前曲 — 已修
- 新增 `BGMEngine::update()`: 曲目播放结束后自动重播 (Sound 无自带 loop)
- `AudioServer::update()` 接入 SceneTree 主循环 (process_frame 每帧驱动)
- 地牢/Boss BGM 不再每 30 秒静音

## Q4.3 拾取反馈 (音效+特效)
- 拾取物品: `play_sfx("pickup")` + ring+spark 闪光 (圣物金色/普通暖色)
- 拾取不再无声无息 (此前仅教程场景有拾取音)

## Q4.4 受击/攻击音效补全
- 新增合成音: `hurt` (玩家受击闷响) + `monster_atk` (怪物攻击嘶吼)
- 玩家受击 2 处 (弹幕/近战) 播放 hurt
- 怪物攻击 (近战/远程) 经 `MONSTER_ATTACK` 事件解耦 — AI 层不持音频引用
- `SceneTree` 注册进 ServiceLocator (事件回调访问音频)
- 新增 `GameEventType::MONSTER_ATTACK`

# v0.9.24 — M4.5 战术链跨场景预测 + M4.4 E2E 验证 (2026-08-07)

## M4.5 跨场景预测 (战术链不再只驱动应付臂)
- `predict_next_action`: 战术链层优先于克隆层 — 预测玩家下一步动作类型
  (SKILL_*→SKILL, COMBO_*→ATTACK); 链 miss 才回落克隆/规则
- `should_interrupt_skill`: 链预测玩家将放技能 (高置信) → 提前进入打断准备
- 新增 `chain_symbol_to_action`/`chain_predict_action` (静态, 单一职责)

## M4.4 E2E 真机路径验证
- `sequence_e2e_test`: 走真实采集链 (PlayerBehaviorRecorder API) → 画像 → 克隆 +
  战术链注入 → 在线观察 → 仲裁/预测/打断, 3 用例 (含技能连发套路)
- 全量 **34/34 绿**

# v0.9.23 — M4.4 战术链序列记忆: 镜像学习玩家战术套路 (2026-08-07)

## M4.4 Tactical Sequence Memory
- **采集层扩展**: `PlayerAction` 新增 `weapon_type` (武器类型) + `combo_stage` (连招段),
  `on_weapon_attack` 传连招段与武器类型 (weapon_executor 调用处已接)
- **TacticalChainTable** (新): 12 战术符号 n-gram (技能×4/位移×4/连招段×3) —
  3-gram 计数表 + 2-gram 降级表, 离线 build (与克隆表同步, F15 enter 注入)
- **降级链**: 3-gram → 2-gram → 克隆表 → 规则; 仲裁链: **ML槽 → 战术链 → 克隆 → Thompson**
- **在线仲裁**: `observe_actual` 维护最近 2 战术符号缓冲 (类型级近似符号), 高置信预测
  玩家下一步战术动作 → 意图 → 应对臂; 缺 skill_id/combo 细节时自然降级不产生错误动作
- **M4.1 验证回归**: 决策抽为 `decide_tactic` 纯函数 + 三场景回归单测 (SNIPER/时停/低血)
- 新增 8 个 tactical_chain 单测 (符号映射/3-gram 计数/2-gram 降级/空流/仲裁), 全量 **33/33 绿**

# v0.9.22 — M5 条件维度: 镜像读懂受压反击/朝向/节奏 (2026-08-07)

## M5 条件维度 (采集 → 统计 → 执行闭环)
- **采集层**: `PlayerAction` 新增 `facing_dir` (朝向) + `hit_in_1s` (近1s受击窗口);
  recorder 新增 `set_battle_context()`, PlayerController 以 HP diff 追踪 1s 受击窗口
- **统计层**: analyzer 新增 3 个真实习惯维度 —
  - `fight_back_rate` 受压反击率 (被打后 1s 内反击占比, 0.6+ 硬刚 / 0.3- 怂包)
  - `face_enemy_rate` 朝向稳定度 (主朝向占比, 高=单向癖可预测退避轴)
  - `attack_rhythm_var` 攻击节奏方差 (相邻攻击间隔 stddev, 小=固定连段可挡)
  - 修正 `player_action.h` 朝向注释 (Direction: 0=下 1=上 2=左 3=右)
- **执行绑定**: M4.1 战术层消费新维度 — 反击型→KITE 拉扯耗链路; 怂+单向癖→
  远程多角度封锁退路; 四面转→贴身缠斗; HUD 画像摘要新增 Counter/Face/Rhythm
- 新增 5 个 analyzer 单测 (反击率/无受击/朝向稳定/节奏方差/少样本安全), 全量 **31/31 绿**
- 克隆表**不加**条件维度: 80 桶已稀疏, 加维度会稀释 (M5 维度走执行层, 不走意图预测)

# v0.9.21 — M4.1/M4.2/M4.3 镜像战术层 (2026-08-06)

## M4.1 战术脚本层
- 新增 `MirrorTactic` 枚举 (OPEN_RANGED/ENGAGE_MELEE/KITE/ADAPTIVE), 画像+态势驱动
  - 玩家 HP<30% → 压进近战终结; SNIPER 或平均距离>260px → 远程消耗 (近身 KITE)
  - aggression>0.55 或 predict_low_dodge → 压进近战; 3s 切换冷却防抖
- 技能选择由循环轮转改为战术映射: 远程→弹幕/AOE, 压进→近战/时停, 拉扯→弹幕/治疗
- `_ai_decide` 首选距离按战术 (260/96/220/200px), 压力探测覆盖 → 80px + 1.3× 攻势加成
- `MirrorAgent` 新增 `profile()` 访问器

## M4.2 镜像专属真冻结
- time_stop: Phase≥2 时冻结玩家 3s (禁移动/攻击/技能), Phase<2 观察期仅减速
- PlayerController 加镜像冻结门控, 冻结期间怪物 AI 与玩家受击保持
- 红霜 overlay 提示, 与玩家时停 (蓝/白) 区分

## M4.3 武器槽切换
- 镜像武器双槽: 近战=玩家武器, 远程=CROSSBOW (倍率×0.8, 射速略慢)
- 战术驱动切换: 远程消耗/拉扯→远程槽, 压进/平衡→近战槽, 2.5s 独立防抖
- 切换重置连招段, 视觉/日志即时反馈

- 全量 **30/30 绿**

# v0.9.20 — 热修复: 玩家时停期间世界未冻结 (镜像/尖刺/弹体/DOT 穿透) (2026-08-06)

## 热修复
- **Bug 复现**: 玩家放 The World 时停后, 镜像 Boss/尖刺/敌方弹体/敌方 DOT 仍在结算 —
  玩家在"时停期间"被镜像伤害击杀 (日志: 镜像 AOE/时停减速照常命中)
- **根因**: 时停门控只覆盖普通怪物 AI (`player_controller.cpp` L94 `_update_monsters`),
  Boss/镜像 (`_boss.tick`)、arena 尖刺毒池、敌方弹体、敌方 buff 四条伤害链全部绕过
- **修复** (game_scene.cpp, 4 处门控 `time_stop_remaining <= 0`):
  - `_boss.tick` 调用 (BossAI/镜像/领域/arena 区域伤害)
  - arena 物体循环 (尖刺/毒池/图腾)
  - 敌方弹体 (MONSTER/ENVIRONMENT owner)
  - 敌方 buff tick (毒 DOT/venom_fang; 玩家自身 buff 不受影响)
- 全量 **30/30 绿** · 桌面已同步

# v0.9.19 — 热修复: 死亡后继续游戏闪退 (EventBus 悬挂订阅) (2026-08-06)

## 热修复
- **闪退根因**: `EventBus::subscribe` 的 `Sub.owner` 从未填充 (写死 `nullptr`),
  且 GameScene 析构不注销订阅 — 玩家死亡 → GameScene (`_gameplay`/`_boss`/
  `_presentation`) 析构后, EventBus 仍保留捕获 `[this]` 的 lambda
- 继续游戏 → 新 GameScene `enter_floor` → `emit(FLOOR_ENTER)` → 调用已析构对象的回调 →
  未定义行为 → 闪退 (首次进 11 层正常, 死亡后再继续必崩 — 与日志完全吻合)
- **修复** (5 文件):
  - `event_bus.h/.cpp`: `subscribe` 增加 `owner` 参数, 正确填充 `Sub.owner`
  - 三个 Director 各加 `unregister_events()` (gameplay: RELIC_GAIN/FLOOR_ENTER;
    boss: BOSS_DEAD/FLOOR_ENTER; presentation: 6 类事件), 订阅时传 `this`
  - `GameScene::~GameScene` 析构时统一注销, 消除悬挂回调
- 全量 **30/30 绿** · 桌面已同步

# v0.9.18 — 热修复: 选关进入普通层闪退 (F9 overlay 空指针) (2026-08-06)

## 热修复
- **闪退根因**: v0.9.17 修改 F9 MIRROR AI overlay 时误删外层守卫,
  `game_scene.cpp` L1545 无条件解引用 `_boss._mirror_agent` — 普通层 (选关11层)
  不创建镜像 agent, `unique_ptr` 为空 → 0xC0000005 (SEH) → 闪退; 15 层 Boss 层 agent
  非空, 故读档从未触发
- 修复: 恢复守卫 `if (g_show_mirror_acc && _boss._mirror_agent && g_font_loaded)`
- 调试工具增强: `seh_handler` 崩溃日志增加 RVA+模块基址 (配合 Debug 构建 addr2line 定位)
- 全量 **30/30 绿** · 桌面已同步 (Release exe 3.3MB)

# v0.9.17 — M4 调参基础设施: MirrorTuning 参数表 + 漂移降权消费 (2026-08-06)

## M4 (第一批: 参数化 + 断链修复)
- 新增 `MirrorTuning` (`src/ai/mirror/mirror_tuning.h`): 全部 Phase 触发阈值/仲裁置信度/漂移降权集中管理 (单例可调), 为实测标定留入口
- **修复第二个"算了没用"断链**: `profile_drift()` 此前零调用方 — 现在被消费:
  - `clone_confidence_threshold()`: 漂移>0.5 → 克隆置信门槛 0.50→0.75 (玩家换打法 → 模仿降权, 交 Thompson 在线适应)
  - predict_next_action / recommend_action 克隆分支改用动态门槛
- **Phase 时间兜底按实战标定**: P1→P2 兜底 20s→12s (实战第1局战斗约20s, 旧值在短战斗几乎必然只走兜底/打不完)
- F9 HUD 加 `Drift:% Bar:` 行 (漂移与当前门槛可视化)
- 单测: 漂移降权 2 项 + tuning 时间兜底可调 1 项, 全量 **30/30 绿** · World Validator 通过 · 桌面已同步
- 待实测第2局: 确认 `[MIRROR] CloneTable built` 非空 + `[MIRROR-ACC]` 摘要 (决定下一批数值标定)

# v0.9.16 — M4 链路线接通: 运行时注入克隆表 (验收发现致命断链) (2026-08-06)

## M4 前置修复 (实战验收第1局暴露)
- **致命断链修复**: `set_clone_table` 在游戏运行时代码**零调用** — 克隆表只在单测注入, 实战 `_clone==nullptr`, Echo 反制全来自规则/画像而非克隆层
- `_init_mirror_boss` 现从 `g_behavior.history()` 构建 `BehaviorCloneTable` (build + set_profile + set_clone_table) 并 LOG `CloneTable built: N entries`
- `[MIRROR-ACC]` 战斗摘要从 printf 改走 `LOG_INFO` → 统计进 `game.log` (不再丢在控制台)
- 30/30 全绿 · 桌面已同步 — **需再实测一局验证 `[MIRROR-ACC]` 摘要与 `CloneTable built` 日志**

# v0.9.15 — F15 M3 后验验收: MirrorDebugStats AI 链路闭环证据 (2026-08-06)

## M3-AC (后验验收, 无新 AI 功能, 只证明链路真闭环)
- 新增 `MirrorDebugStats` (`src/ai/mirror/`): Predict/克隆(精确/模糊)/画像/默认/规则 降级链计数 + 仲裁[Clone/ML/Thompson] + 打断(尝试/成功) + 行为分布(A/S/R/Approach) + 各 Phase 时长
- MirrorAgent 全面打点: predict_next_action / recommend_action / tick_phase 每分支计数 (const 安全, 非侵入)
- Director 打点: 打断尝试 + 行为状态每决策帧采样
- **技能映射核对 (验收点4)**: director case 0-4 全真实效果 (heal=`boss.combat.heal(max/5)`、时停=`slow×4`、近战/弹幕/AOE 真实伤害) — 无"名字镜像"; **修复**: 自愈/时停此前缺 `report_outcome` 在线反馈 → 已补正反馈
- **F9 HUD**: 战斗中 toggle MIRROR AI 统计 overlay (Predict/CloneHit/Rule/打断/行为分布/Phase时长)
- 战斗结束日志: boss_system_director 导出 `[MIRROR-ACC] battle ended — <summary>` (每场只记一次, `begin_battle()` 重置)
- 单测 10 项 (统计逻辑 7 + MirrorAgent 真实路径集成 3), 全量 **30/30 绿**
- 验收手册写入设计文档 §7: 前 14 层埋"低血回血"习惯 → F15 按 F9 验收克隆驱动/调用链/Phase 行为/技能真实效果
- 已知缺陷记录: `--sim` 需标题画面手按 N (G5.6 无自动开始), 无人值守验证不可用 → 验收需人工实操; 若 Predict=0 则停止 M4 · 桌面版已同步

## Bugfix: sim/正常退出不再崩
- **根因 (gdb 栈回溯定位)**: main.cpp 显式 `ResourceManager::inst().unload_all()` 后, 静态单例析构再调一次 `unload_all()` → 二次 `UnloadFont` → 字体 double-free → 堆损坏 (Release 0xC0000409 / Debug 0xC0000374), 崩在程序退出阶段
- 修复: `unload_all()` 加 `_loaded` 防重入保护 (一次性卸载), 二次调用直接返回
- 验证: `--sim 1` 退出码 0 (修复前稳定崩溃), Debug+gdb backtrace 确认崩溃帧 = 单例析构卸载字体; 29/29 全绿
- 顺带: `.gitignore` 补 `build-dbg/` · 桌面版已同步

## M3: 克隆层接入行为选择仲裁 (G5)
- `recommend_action` 仲裁链: **ML 插槽 (G5, 注册即启用, 默认关闭)** → **克隆层 (Phase≥2, 置信度>0.5 驱动行为臂)** → **Thompson 采样** → 规则兜底 (观察期)
- 玩家意图 → Boss 应对臂映射 (镜像反制语义): HEAL/DODGE/RETREAT→压近惩罚, SKILL→技能打断, ATTACK→连招, ADVANCE→拉扯
- `_record_arm` 统一记录臂+上下文桶, 保持 `report_outcome` 在线反馈链完整
- `set_ml_predictor(std::function<PlayerActionType(state)>)` 插槽预留 (G5), 默认 nullptr 关闭
- 单测 6 项 (高/低置信度仲裁、ML 覆盖克隆、非决策忽略、观察期不介入), 全量 29/29 绿
- ⚠️ 已知问题: `--sim` 冒烟崩 (0xC0000374 堆损坏) 为**既有缺陷** (M2 exe 复现一致), 待独立修复, 与 M3 无关 · 桌面版已同步

## M2: Phase 1-2-3 从纯计时改为数据驱动
- 新增 `RollingAccuracy` (`src/ai/mirror/`): 32 次滑动窗口在线命中率, 只关注近期表现
- **动态 Phase 触发** 替代 `tick_phase_timer` (删除死代码与相位计时字段):
  - P1→P2: 准确率≥0.65 且观察≥20 / 观察≥40 / 战斗时间≥20s
  - P2→P3: 同桶命中≥10 且准确率≥0.7 (核心模式) / 玩家或BOSS HP<35% (濒危)
- **在线观测**: MirrorAgent 新增 `on_prediction`(附 ObservationKey 上下文) + `observe_actual`(玩家实际动作反馈), 命中/落空滚窗统计
- **画像一致性**: `profile_drift` — 当前战斗攻击/技能频率 vs 画像频率归一化偏差 [0,1]
- MirrorCombatDirector 集成: 每帧识别玩家实际动作 (攻击/技能/闪避位移/喝药HP上升) → 反馈观察器; 预测后立即上报上下文
- BossSystemDirector 每帧动态判定 (传 HP 快照)
- 新增 `player_action.h::is_decision_action()` 语义化过滤 (ATTACK/SKILL/DODGE/HEAL)
- 单测 12 项 (滚窗滑动/触发阈值/低准确率滞留/漂移计算), 全量 28/28 绿 · 桌面版已同步

## M1: Player Clone Agent 第一层学习模块
- 新增 `BehaviorCloneTable` (`src/ai/mirror/`): 从 F1-F14 PlayerAction 流构建 state→意图分布, 零神经网络
- **可解释 ObservationKey**: `"d<距离桶>:h<血量桶>:s<技能就绪桶>"` (d: 贴身/近/中/远/极远, h: 危急/低/中/高, s: 就绪技能数)
- **战斗意图枚举 PlayerIntention** (7 类): ATTACK/SKILL/DODGE/HEAL/ADVANCE/RETREAT/IDLE — 非"简单 ATTACK/SKILL"
- **4 级降级链**: 精确状态 → 模糊状态(合并技能维度) → PlayerHabitProfile 规则 → 默认策略
- PlayerAction 扩展响应上下文快照 (hp / enemy_dist / skill_ready_mask), recorder `set_context` 每帧注入 (player_controller), 旧流向后兼容 (-1 = 未知)
- MirrorAgent 集成克隆层: Phase≥2 优先查表 (置信度≥0.5), 规则层兜底; `MirrorBattleState` 加 `player_skills_ready`
- 单测 9 项 (含验收: 低血+近距离+技能Ready → 预测 HEAL), 全量 27/27 绿 · 桌面版已同步

## 稳定性修复
- **数据加载器幂等化**: enemy/boss/skill/item/buff/relic 的 `load_xxx_defs` 统一补 `|| is_xxx_defs_loaded()` 快路径, 重复加载不再触发 MergeMode::Skip 空档
- **World 加载器指针悬垂修复**: biome/encounter/hazard/landmark 从 “push_back 后取 `&back()`” 改为 "先 push 全部再建索引", 消除 vector 扩容导致的悬挂指针
- `item_defs` 流读取顺序修复 (先读全文再 parse, 避免 `f >> j` 后迭代器读到空)
- `WeaponSpecialState::should_fire_next`: 连击末击后去激活但保留 `hit_count/tracked`, 修复第 5 击伤害错用第 1 档倍率
- `AttackContext::valid()` 补 `t >= timestamp` 过滤, 未来时间戳不再判定有效
- CMake `enable_testing()` 补全 (ENABLE_TESTS 分支)

## 测试套件 26/26 全绿
- save_test 重写为自足 roundtrip (原依赖运行时生成的 `saves/` 产物)
- astar "不可达" 用例改为 3×3 墙环孤岛 (原包围圈逻辑实际可达)
- 同步过时断言: observation 8 特征/999 哨兵, element 冰冻曲线 (Lv6≈33.7), sim 浮点序列化, q_agent 空状态 ATTACK 合法性, buff DOT 末档计数, mcts 邻近怪物收敛, condition 空串语义
- World Validator 0 错误 · Release 构建 100% · --sim 20 冒烟无崩 · 桌面版已同步

## 素材覆盖补齐最后一块
- `Monster.sprite_override`: 素材 key 覆盖字段 — Boss 工厂按层指定 (F5→boss_f5 暗影骑士图, F10→boss_f10 地狱火魔图, F15→boss_self 玩家形象), 降级路径默认 F5 形象
- `_monster_sprite_key()` 改为优先 override; Boss 也走数据驱动素材 (程序化占位此前无 Boss 专属差异)
- 特殊房间中心: 祭坛/宝箱/泉水 中心图标从字符 (+, $, ~) 升级为素材精灵 (altar/chest/spring_top, 0.75× 缩放), 触发后仍显灰字; 其余房间 (商店/铁匠/图书馆/赌徒/圣地/秘室) 维持字符
- 构建 100% · 冒烟 5s 无崩 · 桌面版已同步重编译

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
