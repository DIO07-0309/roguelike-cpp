# 武器三连击系统 — 设计思路

## 一、问题

G9 之前，所有武器只有一种普攻：以玩家为圆心、1.5 格半径的圆形判定，配合一个全局 `ComboState`（1→2→3→4 段，伤害系数 1.0/1.15/1.4/2.2）。

**问题不是伤害不够，而是所有武器玩起来完全一样。** 拿起匕首和拿起长剑没有任何区别——你只是在等待技能 CD，普攻是填充物。

## 二、核心思路：武器决定玩法

> "Weapons determine gameplay, not skills." — G9 Design Doc Principle 1

`combo_index` 不是全局的，而是**每种武器独立定义**。剑的三段和匕首的三段可以截然不同——不仅是伤害数值，还有攻击形状、距离、后摇、震屏强度、音效。

### 数据结构

三层分离：

```
WeaponDef         — JSON 数据，只读，永不变
  └── AttackStageDef[3]  — 每段的数值（倍率/形状/距离/后摇）

WeaponRuntime     — 运行时状态（combo_index, timer, recovery, fatigue），不存盘
  └── WeaponSpecialState  — 多段特殊攻击（双截棍 5 连打、长矛 10 连刺）

WeaponExecutor    — 静态方法类，读 WeaponDef + WeaponRuntime → 检测 → 伤害 → 推进
```

没有 `WeaponManager`、`ComboManager`、`AttackManager`——G1~G8 的教训是 Manager 最终都会变成 God Object。

## 三、combo 生命周期

```
玩家按空格
  │
  ▼
PlayerController::player_attack()
  │ 检查 player->weapon.can_attack(game_time)
  │   └── recovery_timer == 0 && !fatigue && combo_cooldown
  │
  ▼
WeaponExecutor::execute()
  │ 读当前 WeaponDef + AttackStageDef
  │
  ├─→ hit_detect_by_shape()   ← 圆形/扇形/矩形/胶囊/弹道
  │
  ├─→ calculate_damage() × stage.damage_multiplier
  │
  ├─→ WeaponComponent::execute_attack()
  │     ├── recovery_timer = stage.recovery (只在这个窗口内无法攻击)
  │     └── combo_index = (combo_index + 1) % 3
  │           combo_timer    = def->combo_timeout (超时→归零)
  │
  └─→ 返回 AttackResult[] (调用方负责 VFX/震屏/数字)
```

关键设计：**advance 发生在 execute 之后，PlayerController 在调用前读 stage**。如果反过来，反馈就错位了。

## 四、为什么每把武器感觉不同

### 不是数值不同，是节奏不同

以 Dagger 为例：

```
Stage[0]: recovery=0.20s, hit_shape=SECTOR, range=1.0, width=90°
Stage[1]: recovery=0.25s, hit_shape=SECTOR, range=1.0, width=60°
Stage[2]: recovery=0.35s, hit_shape=CAPSULE, range=1.5, mult=1.15
```

- Stage 1 很快（0.20s 后摇），下一个按键窗口紧
- Stage 2 稍慢（0.25s），手感到变化
- Stage 3 最慢（0.35s）但倍率最高（1.15） + 胶囊判定=突刺穿透

**玩家不用看 UI，只听音效节奏 + 看 VFX 就知道现在是第几段。**

以 Sword 为例：

```
Stage[0]: recovery=0.35s, HIT_SHAPE=RECTANGLE, range=2.0, mult=1.20 (跳斩)
Stage[1]: recovery=0.25s, HIT_SHAPE=SECTOR,   range=2.0, mult=1.00 (横扫)
Stage[2]: recovery=0.45s, HIT_SHAPE=CAPSULE,  range=2.0, mult=0.80 (震地)
```

剑的 Stage 1 反而是最慢的（跳起来落下去），Stage 2 变快（横扫），Stage 3 最慢但附带眩晕。节奏是"慢→快→慢"，和匕首的"快→中→慢"完全不同。

### 不是所有武器都有 3 段

```
Fist:      Stage[0] only  — 无 combo
Dagger:    Stage[0,1,2]   — 3 段
Sword:     Stage[0,1,2]   — 3 段
Nunchaku:  Stage[0,1,2]   — 3 段，Stage[2] 触发 WeaponSpecialState
Spear:     Stage[0,1,2]   — 3 段，Stage[2] 触发 WeaponSpecialState
Crossbow:  Stage[0,1,2]   — Stage[2] 发射穿透弹 + 进入疲劳
```

### 特殊攻击不是另起炉灶

Nunchaku 和 Spear 的 Stage 3 不需要单独的 AttackManager。

```
WeaponSpecialState sp;
sp.start(max_hits, interval_ms, base_multiplier, growth);
// 存在 WeaponRuntime 里，每次 tick 检查 should_fire_next()
// GameScene::_process() 每帧调用 WeaponExecutor::tick_specials()
// 每触发一次 → hit_detect → damage → 返回 AttackResult
```

对调用方来说，tick 返回的结果和普通攻击完全一样——同一个 `WeaponAttackResult` 结构体。VFX 处理在 `game_scene.cpp` 里统一分发。

## 五、命中判定解耦

`hit_detection.cpp` 是纯几何库，不依赖 Player/Monster/CombatSystem。

```cpp
// 五种形状，同一接口
std::vector<HitResult> hit_detect_circle(origin, radius_px, targets);
std::vector<HitResult> hit_detect_sector(origin, dir, radius, half_angle_deg, targets);
std::vector<HitResult> hit_detect_rectangle(origin, dir, length, width, targets);
std::vector<HitResult> hit_detect_capsule(origin, dir, length, radius, targets);

// 统一分发
std::vector<HitResult> hit_detect_by_shape(hit_shape_enum, origin, dir, range, width, targets);
```

新增形状（扇形→巨剑、胶囊→链鞭、矩形→激光）不用改 WeaponExecutor。

## 六、VFX 不在 WeaponExecutor 里

这是最关键的设计决策。WeaponExecutor **只管数学**——判定、伤害、推进 combo。

VFX 完全由调用方（`PlayerController::_weapon_attack`）决定：

```cpp
switch (weapon_type) {
case WeaponType::SWORD:
    if (stage == 0) { shockwave + slash_arc + smoke }      // 跳斩
    else if (stage == 1) { double_slash_arc + spark_burst }  // 横扫
    else { shockwave(80px) + explosion + smoke + flash }     // 震地
    break;
}
```

这意味着将来如果换了渲染引擎，只需要改 `_weapon_attack` 里的 VFX 分发，WeaponExecutor 一行不动。

## 七、AI 不知道武器

`DecisionAgent`、`BehaviorTree`、`MCTS`、`RL` 全部只调用 `"attack"` action。它们不知道玩家拿的是什么武器、现在是第几段、判定形状是什么。

```
SimAgent::is_action_just_pressed("attack") → 空格
  → PlayerController::player_attack()
    → WeaponExecutor::execute()
      → 自动读当前武器/阶段的定义
```

这也是为什么 G9 能在 G8 的 AI 模块上做零改动集成。

## 八、数据流全景

```
resources/weapons.json          ← 24 条目，设计师改
  │
  ▼ (启动时 load_weapon_defs)
WeaponDef Registry              ← 静态数据，只读
  │
  ▼ (捡起武器 → Inventory::equip → WeaponComponent::equip)
Player::weapon                  ← WeaponComponent，带 WeaponRuntime
  │
  ▼ (空格键)
PlayerController::player_attack
  │
  ├→ WeaponExecutor::execute    ← 判定 + 伤害
  │     ├→ hit_detect_by_shape
  │     ├→ calculate_damage × stage.multiplier
  │     └→ weapon.execute_attack (推进 combo)
  │
  └→ VFX 分发 (每武器每阶段独立)
        ├→ VFXServer primitives (beam/ring/shockwave/slash_arc...)
        └→ 震屏 / 冻结
```

## 九、给要扩展的人

新增第 7 种武器只需：

1. `resources/weapons.json` 加 4 条（common/rare/epic/legendary）
2. `resources/items.json` 加 1 条标记
3. `_weapon_attack()` 的 switch 加一个 case 写 VFX
4. 如果 stage-3 有特殊机制，在 `WeaponExecutor::execute()` 的 stage-3 initiation 块加一行

不用改 CombatSystem、不用改 AI、不用改存档逻辑（`wpn:` 字段自动持久化）。

新增第 4 段连击：

1. 把 `stages[3]` 改成 `stages[N]`，`N` 写进 `stage_count`
2. `WeaponRuntime::advance()` 改成 `% stage_count` 替代硬编码 `% 3`

框架已为此预埋。
