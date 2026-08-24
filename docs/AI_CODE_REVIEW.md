# Roguelike C++ AI 系统代码级深度审查报告

> 审查范围：src/ai/ 全部 + src/core/sim/ + src/game/ai/player_behavior/
> 方法：逐文件源码级审查（带文件:行号证据）
> 日期：2026-08-25 | v0.9.34 已修复其中 11 处（见 CHANGELOG），其余记录在案
> 配套：roguelike-ai-master-guide.md（桌面学习手册）

---

## 执行摘要

| 模块 | 文件数 | 高危问题 | 成熟度 |
|------|--------|----------|--------|
| DecisionAgent（sim_ai） | 4 | P0-3(已回退) P1-2(已修) | ★★★★☆ 实战淬炼 |
| BTAgent + 行为树 | 11 | **P0-1/P0-2（已修，原致命）** | ★★→★★★★ |
| MCTS | 8 | A1/A6（已修）A2/A3(已修) | ★★★☆☆ 原型质量 |
| Q-Learning | 9 | B1/B2/B3（已修） | ★★★☆☆ 原型质量 |
| MirrorAgent 镜像系统 | 13 | MP1（已修）MP2-MP4 记录在案 | ★★★★☆ 架构最佳 |
| A* 导航 | 2 | 无边界校验（记录在案） | ★★★★☆ 未接入 |
| 行为画像管线 | 8 | 特征缺陷 3 处（记录在案） | ★★★☆☆ |

---

## 一、DecisionAgent（Utility AI）

### 架构
argmax 选择器（sim_ai.cpp:624-696）；Build 感知画像参数表（:73-131，_prefer_range/_prefer_aoe/_prefer_skill/_prefer_heal）；Move 内部优先级阶梯（躲招1.4 > 脱毒1.2 > 攻击≤1.0 > 拾取1.5×威胁衰减 > 搜索0.8/0.6 > 危险-1.0 > 不可行-999）。

### 底层导航栈
_bfs_toward（首步传播技巧）/ _bfs_away（怪物源距离场）/ greedy_step 三级降级；路径记忆（instance_id 键，消 BFS 等权震荡）；卡死逃脱状态机（≥2s 方向轮换 / ≥8s 传送兜底，怪方 HP 变化区分换血僵持）；同帧一致性缓存（Q3.1）。

### 问题与处理
- **P0-3 攻击死区**（sim_ai.cpp:148 vs :471-475）：[48px, ideal] 区间 attack/move 双零，远程死区 ~90px；"拉开距离"分支条件是站桩区间子集 = 永不可达死代码。**修复尝试激活后 200 局 10%→3.5%（风筝震荡破坏平衡），已回退并在注释中记录**。
- **P1-2 治疗优先级倒置**（:643-653）：自愈槽遍历无 break → **已修**。
- P1-1 口袋传送第二遍循环为逐字节相同死代码、落点可踩熔岩（is_rect_walkable 只查墙）（:206-220）— 记录在案。
- P1-3 搜刮看门狗跨楼层不复位（sim_ai.h:100-101）— 记录在案。
- P1-5 每方向独立重跑全图 BFS 最坏 ~12 次/帧 — 记录在案（优化方向：提升到 tick 层每帧每类一次）。
- P2 系列：路径记忆绕过危险规避(:494-498)、技能评分无 LOS/无个性(:161 穿墙计数)、_aggro_bias 死参数、pixel_to_tile 负坐标截断(game_map.cpp:28-30)。

---

## 二、BTAgent 与行为树

- **P0-1 接线致命伤（已修）**：actConfirm/actDescend 创建后零引用，根节点 push 裸 Condition（bt_agent.cpp:162-163）→ Selector 命中即短路返回默认 move_up，BT agent 永远无法下楼/确认。修复：Sequence{cond, act}。
- **P0-2 can_use(0) 时间语义（已修）**：开局恒真（假阳性）、用过一次后永久假阴性。修复：BTAgent::set_time 注入链 + can_use(_game_time)。
- P2-4 无同帧缓存（DecisionAgent 的 Q3.1 模式缺失）；P2-5 accept_event 文案匹配漂移("skill" vs "skill_level")；P2-6 距离用左上角差/heal_ready 不验槽位类型。
- 升级方向：RUNNING 语义 + Decorator（Cooldown/Retry/Parallel）；两份 profile 表已漂移应合并。

---

## 三、MCTS

实现：UCT C=1.414（mcts_node.h:20）、expand-all（mcts_search.cpp:171-182）、rollout 深度10 `(rng+depth)%n`、backprop 折扣 0.95（:202）、终局评估 ±1000/+200击杀/hp×2−怪hp×1.5−depth×5。

- **A1 奖励量纲失配（已修）**：C=√2 前提是 [0,1] 奖励；±1000 下探索项上界~2.5 永远翻不动利用项 → UCT≈纯贪心。修复：sigmoid(score/250) 归一化。
- **A2 WAIT 首轮偏差（已修）**：expand-all 后恒取 children.back()=WAIT → 迭代序轮换。
- **A3 回传折扣破坏均值可比性（已修）**：删除 0.95 衰减。
- **A6 快照伪造禁普攻（已修）**：attack_cooldown 恒 0.5 → 根节点合法动作集永不含 ATTACK。修复：真实 remaining_cooldown（build_sim_state 加 game_time 参数）。
- 失真记录在案：buffs/mdef 克隆但不消费、怪物永不动、反击恒 40%、穿墙合法、RNG 种子由像素坐标派生、RNGState.calls 死字段。
- 优化方向：单子扩展、树复用（现在每决策帧丢弃整棵树）、截断 rollout 用评估函数打分、Solver 化。

---

## 四、Q-Learning

实现：Q 表键 "<hp>:<atk>:<ec>:<dist>:<boss>:<style>|<action>"（observation.cpp:42-53，atk/ec/dist 桶无上限、哨兵 999 入桶49）；Observation 向量实际 8 维但 to_key 只用 6 维（buff_count 丢失）；α=0.1/0.15 γ=0.9 ε 0.12→0.005。

- **B1 终局自举污染（已修）**：update 缺 done — "键不存在当终止"，真终局反而自举。修复：done 参数 + rl_runner 传 env.is_done()。
- **B2 学习率不衰减（已修）**：违反 Σα²<∞。修复：α/(1+0.05·visits)。
- **B3 击杀奖励按尸体存量（已修）**：prev_alive 死变量 + 每步每尸体 +50 → 回报结构与剩余步数耦合。修复：增量式一次性发放。
- B4-B8 记录在案：ε 续训重置、LCG 词复用两次决策相关、哨兵伪共享桶、镜像 reward shaping 失真（base 当伤害/dodged 恒 true/AGGRESSIVE 常数偏置/panic-heal 不可达）、训练场景单一过拟合、action_distribution 子串匹配 "|1" 误配 "|11"。

---

## 五、MirrorAgent 镜像系统

架构亮点：双链分离（预测链 vs 仲裁链各自降级）；五层仲裁 L0 ML→L1 战术链(>)→L2 RL(≥0)→L3 克隆(level≤1,>)→L4 Thompson 兜底（含40%施法旁路）；三阶段参数 mirror_tuning.h 单例；全链路可观测（mirror_debug_stats）；幂等反馈防护(_last_bucket=-1)；Marsaglia-Tsang Gamma 完整实现（a<1 boost + squeeze）。

- **MP1 先验爆炸（已修）**：init_prior +2 每局随存档固化 + import 纯加法 → α≈1+2(k+1)+Σr 线性膨胀 → 后验方差 ∝1/(α+β)² → 探索归零；float 到 2²⁴ 后 += 静默失效。双重修复：export 扣除 pending 先验 + update 全臂折扣 λ=0.995/floor 0.25。
- MP2 off-policy 反馈污染：report_outcome 更新的臂无论决策来自哪层（RL 层偏好污染 Thompson 统计）— 记录在案（IPS 校正或分层统计）。
- MP3 在线战术链符号坍缩：在线只产 {SKILL_0, COMBO_1} 两符号（键空间 1331→8），离线表大半死数据（mirror_agent.cpp:226-232）— 记录在案。
- MP4 固定优先级短路无融合；门槛比较方向不一致（>= vs >）；profile 置信硬编码 0.5 恰压线通过门槛（漂移上浮到 0.75 时预测链 profile 层永久失效）。
- MP5 NaN 导入可致 Gamma 采样死循环（存档解析无校验）。
- MP6 n-gram 硬切换降级而非 backoff 平滑；小样本 confidence 达 1.0 无收缩（Laplace/Wilson 下界）。
- MP7 漂移检测相对误差对小基率敏感必误报、无统计检验（CUSUM/Page-Hinkley 升级方向）。
- 文档 bug："144 float" 实为 36 floats × 2 向量（字节/浮点混淆，online_adaptive_policy.h:38）。

---

## 六、A* 与行为画像

A*（pathfinder.cpp）：扁平 SoA + 惰性删除 priority_queue + 可采纳启发式 + WalkableFn 依赖反转 + 7 维测试。问题：start/goal 零边界校验（对比 sim_ai 四处 clamp 血泪防御——同一仓库两种标准）；find_path 仅测试引用（建成未通车）。

行为画像：采集协议优秀（上下文回填/MOVE 去重/实时聚合）；特征缺陷记录在案：retreat_rate 用绝对上方向（facing_dir 已采集未用）、fight_back_rate 可>1、avg_damage_taken 分母错、hp 字段未消费、JSON 序列化丢 M1/M5 特征且 load 空 stub。

## 七、SimRunner

亮点：配对种子（all-builds CRN 方差削减）；末段 10% 低探索收敛窗口。问题：EnemyStats.kills 无写入点（威胁报告恒空）、finalize N=0 除零风险、avg_turns 实为帧数、death_floor_dist floor>15 静默丢弃。

---

## 修复验证（v0.9.34）

编译 0 警告 | 34/34 ctest | validator 0 error | **500 局回归 9.0%（45/500，区间 6-10%）**

## 修复优先级路线图（剩余项）

**第三批（性能结构）**：BFS 提升到 tick 层每帧一次；MCTS 树复用；激活 hp 字段修 retreat_rate/fight_back_rate；A* 边界校验统一防御标准；NaN 导入防护。
**第四批（升级实验）**：Thompson→LinTS；n-gram→Katz backoff；仲裁链→MoE 门控；Q 表→DQN；MCTS 评估→神经网络。
