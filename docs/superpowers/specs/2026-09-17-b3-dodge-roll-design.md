# B3 翻滚/闪避 — 设计规格 (v1.7-B3)

> 状态: 待用户审阅 · 日期: 2026-09-17
> 来源: docs/V1_6_ROADMAP.md 决策点 #2 已拍板 — "纯手感位移"定位
> (无无敌帧, 不改战斗数学; 表现件 B 案 = 形变+尘土+残影一步到位)

## 1. 定位与决策记录

| 决策 | 结论 | 理由 |
|:---|:---|:---|
| 机制定位 | 纯手感位移, 无无敌帧 | 防御资源会改受击数学 → 500 局基线全量重调; 本批只做"第四件手感件" |
| 键位 | Shift(任一) + 方向键 | InputMap 加一个 dodge 动作, 动作游戏主流惯例, 零改键系统 |
| Mirror | 采集接入 (显式 on_dodge 钩子) | DODGE 是 CloneTable 7 意图之一 + 画像"闪避习惯"维度均已存在, 零学习侧新代码 |
| sim | **不加** dodge 动作 | 500 局基线保持可比; sim 不翻滚 → 与 B3 前报告逐字节一致是验收项 |
| 表现件 | B 案: 形变 + 尘土 + 残影 | 用户拍板 (2026-09-17); "纯手感"没有表现层 = 幽灵步, 不可交付 |

## 2. 机制参数

| 参数 | 值 | 备注 |
|:---|:---|:---|
| 触发 | `dodge` 动作 edge-press | LEFT/RIGHT_SHIFT 两键并注册 |
| 方向 | 按住的移动轴 (8 向归一化); 无输入 → `player.direction` | 与移动读取同源 |
| 位移 | 2 格 = 64px (TILE_SIZE=32) | 均速 400px/s ≈ 步行 2.0× |
| 时长 | 0.16s (~10 帧 @60Hz 定步长) | dt 驱动, 禁墙钟 |
| 冷却 | 0.7s 独立计时 | 不读不写武器 recovery_timer |
| 翻滚中 | 屏蔽移动输入; 受击**不**打断; 攻击键不吞 (落地后自行再按, v1 无缓冲) | |
| 撞墙 | 沿方向 `_try_move_axis` 贴墙早停, 状态正常收尾, 冷却照旧 | 复用现成碰撞二分 |
| 禁触发 | UI 开启/时停冻结/传送演出 — 复用现有 `can_act` 类门控 | game_scene_input 入口已拦 |

## 3. 架构 (规则 2/3: 单一职责 + 组合)

**新文件 `src/game/systems/dodge_component.h/.cpp`** (与 weapon_component 同级, 零 RNG, 纯 dt):

```cpp
class DodgeComponent {
public:
    bool try_start(const Vector2& dir_norm);  // 冷却 OK 则起翻, 返回是否触发
    void tick(float dt);                      // 推进位移/冷却/残影采样(每 2 帧)
    bool active() const;                      // 翻滚中 (controller 据此接管移动)
    Vector2 delta_this_frame() const;         // 本帧应位移 (px)
    float  remaining_cd() const;              // HUD 备用, 本批不画
    // 表现件查询 (renderer 用):
    float  tilt_deg() const;                  // 0→12→0  ease-out
    Vector2 squash_scale() const;             // (1.10,0.85)→(1,1) 回弹
    const std::vector<RollGhost>& ghosts() const;  // ≤3 个 {pos, alpha}
    void reset();                             // 换层/重开调用
};
```

**接线点 (全部既有文件, 小 diff):**

| 文件 | 改动 | 预估 |
|:---|:---|:---|
| `src/core/input_map.cpp` | setup_defaults 注册 dodge=两 Shift 键 | +3 |
| `src/game/player_controller.cpp` | handle_input 起翻分支 (try_start + 显式 `g_behavior.on_dodge`); tick 中 active→delta 接管移动, else 正常移动 | +45 |
| `src/game/entities/player.h` | 组合 `DodgeComponent dodge;` 成员 | +2 |
| `src/game/rendering/game_renderer.cpp` | 玩家 DrawTexturePro 叠 tilt/scale + ghosts 循环 (alpha tint) | +40 |
| `src/game/rendering3d/hd2d_renderer.cpp` | billboard 顶点面内旋转 (tilt) + 缩放; ghosts → 额外 alpha quad | +45 |
| `resources/vfx_recipes.json` | "dust" 预设 + "dodge_roll" 配方 (起尘+落尘) | +12 |
| `src/game/player_controller.cpp` 或既有 vfx 发射点 | 起/落两拍 emit `dodge_roll` | +8 |
| 3D 环境粒子 (A2 系统) | 起/落 soft-glow burst 6-8 粒 | +15 |
| `tests/` | dodge_component_test 新文件 | +60 |
| 帮助页/README 操作表 | Shift 翻滚一行 | +4 |

合计 ~280 行 ≤ 300 上限。函数全部 ≤40 行 (起翻/接管段各自拆私有小函数)。

## 4. 表现件规格 (B 案)

- **形变**: 起 0→12° 倾斜 (翻滚方向侧), 横 110%/纵 85% 压扁; 后 40% 时长 ease-out 回弹。2D 用 DrawTexturePro `rotation` + `dst` 缩放; 3D 在 billboard quad 顶点做面内旋转 (绕 quad 中心)。
- **残影**: 每 2 帧缓存 {pos, tilt} 最多 3 个, alpha 120/80/40 递减, 同贴图白 modulate 剪影; 起翻 0.1s 内自然排空 (不主动清, 让尾影拖出)。
- **尘土**: 2D 端 `dodge_roll` 配方 (preset "dust" 灰白 [190,180,165] 系, pulse+spark 低幅) 起步爆点 + 落地尾尘; 3D 端 A2 单批软光 mote 一次性 6-8 粒, 群系色自动。
- **音效**: 不新增资源; 若现有 SFX 集有 whoosh/swing 类可复用则落地轻音, 否则静音 (实施计划时查证, 不阻塞)。

## 5. 确定性与存档红线

- DodgeComponent: 零 RNG、dt 定步长驱动、无墙钟 → 同 seed 逐字节复现保持。
- `g_behavior.on_dodge(time,floor,px,py)` 每翻滚仅 1 次 (触发帧); 每帧位移 ~6.7px « 200px 自动通道阈值, 无双记。
- sim: DecisionAgent 动作集不变 (无 dodge) → Agent 永不翻滚; `reports/balance_report.json` 与 B3 前 m4 基线快照**逐字节一致** = 回归验收项 (若不一致 = 有逻辑泄漏进 sim 路径, 必修)。
- Mirror 跨局记忆 (mirror_memory.json) 沿用 `MetaSystem::g_readonly` 屏蔽语义, 无新字段。
- 存档: 无新持久字段, 冷却/翻滚态为局内态, save/load 零迁移; `DodgeComponent::reset()` 挂进既有的换层/重开清理链。

## 6. 测试计划 (ctest 61 → 67)

| # | 用例 | 断言 |
|:---|:---|:---|
| 1 | try_start 成功后 | active()==true, 累计位移=64px±1, cd 归 0.7 |
| 2 | 冷却中再 try_start | 返回 false, 状态不变 |
| 3 | 方向映射 (由 controller 侧纯函数测) | 无输入→direction; 斜输入归一化 |
| 4 | tick 推进 | 0.16s 后 active=false; tilt/scale 回 0/1 |
| 5 | 残影采样 | 每 2 帧 1 个, 上限 3, alpha 递减序 |
| 6 | 撞墙早停 (注入不可走 delta) | 贴墙停稳, 不越界, 正常收尾 |

集成冒烟: hidwin `--autoshot` 抓图 (翻滚中 Shift 模拟不进 autoshot, 以 pixel 扫描 dust 颜色族 + 实机手感为准 — 用户验收)。

## 7. 验收门禁清单

1. Release 0 error · ctest 67/67 · world_validator 0/0 (vfx_recipes 新增)
2. sim 双跑字节一致 + 对 m4 基线快照 diff = 空
3. 2D/3D 双模式实机: Shift 翻滚手感/尘土/残影/落地回弹 (用户)
4. Mirror HUD (B1.1) 可见 DODGE 意图计数起算
5. 桌面包同步 + CHANGELOG + README 操作表 + V1_6_ROADMAP B3 行状态

## 8. 明确不做 (YAGNI)

改键 UI · 攻击取消翻滚窗口 · 耐力条 · 翻滚无敌帧 · sim 对称 · 输入缓冲 · 专用翻滚贴图帧 · 翻滚穿怪 (v1 视地图规则同墙)

## 9. 实施勘误 (writing-plans 阶段定稿, 2026-09-17)

1. **尘土不新增 vfx_recipes.json 配方** — 改用既有直发模式 `VFXServer` ring+spark_burst → `gs.active_effects` (player_controller.cpp:470-473 同构)。3D 侧经 `_build_effects` 自动消费, **删** §4 的 A2 mote burst 专属改动; validator 门保留为 T5 全量形式合规 (JSON 实际零改动)。
2. **3D 不做倾斜旋转** — `DrawBillboardRec` 无 rotation 参数; 3D 翻滚表现 = squash 缩放 (`HD2DDrawItem.scale_w/scale_h`) + 残影 quad。倾斜仅 2D (脚底 origin 现成路径)。
3. **撞墙早停单测并入实机验收** — `_try_move_axis` 二分贴墙是既有已验证逻辑, 组件层恒定 delta 已由 `RollMovesFullDistanceThenEnds` 覆盖; 计划 ctest 新增用例仍为 6 (61→67 不变)。
4. **换层/重开重置链** — `dodge.reset()` 挂 `Player::reset_attack_timers()` (player.cpp:58, 进层已被 game_scene.cpp 调用), 不新开钩子点。
