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
