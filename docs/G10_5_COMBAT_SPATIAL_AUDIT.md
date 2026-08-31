# G10.5-A — Combat Spatial Audit (Design Gate)

> 状态: **待 Review — 未批准前不写代码**
> 日期: 2026-08-31
> 目标: 确认「角色模型 → 攻击判定 → 攻击特效」是否共享同一空间真相
> 方法: 全链路只读审计（entity 坐标语义 → hit_detection 几何 → VFX 生成点逐一对数值）

---

## 1. 核心结论

> **坐标系本身是统一的（P2 排除大半），问题在"两条独立数据链"**：
> - 判定几何 = `weapons.json (range/width)` → `hit_detection`
> - 视觉几何 = `player_controller.cpp` 硬编码 switch + `vfx_recipes.json`
>
> **两者之间零共享**。调 JSON range 不会传导到 VFX；VFX 半径是拍的，判定半径是算的。
> 弓弩是全项目唯一 SSOT 样板——弹体 `pos` 单字段同时喂碰撞与渲染。

---

## 2. 玩家空间模型（坐标系审计 ✅ 基本健康）

| 概念 | 定义 | 位置 |
|------|------|------|
| `position` | **视觉精灵左上角** | entity.h:16 |
| 视觉中心 | `position + (16,16)` | 32×32 精灵 |
| 碰撞中心 | `position + (2+14, 2+14)` = `position+16` | 28×28 内嵌于 32×32 |
| **Attack Origin** | `rect.x + rect.width/2`（碰撞中心） | weapon_executor.cpp:80-83 |
| **VFX Origin** | 同公式 | player_controller.cpp:609-610 |
| Monster 判定点 | `rect` 中心（= position+14） | hit_detection.cpp:19-24 |

**视觉中心 = 碰撞中心 = Attack Origin = VFX Origin，四点重合。** 玩家不存在中心错位——P2 类问题全部来自下文的装饰偏移与 fudge 常数。

---

## 3. 全武器判定 × 视觉对照表（核心交付）

单位 px；"前伸" = 判定/视觉沿朝向最远边界。

| 武器段 | 判定 shape/range | VFX range | 对齐结论 |
|--------|-----------------|-----------|---------|
| dagger s0 | SECTOR 32px ±90° | 弧 50px | ❌ **视觉超判定 18px**（"砍中没伤害"带） |
| dagger s1 | SECTOR 32px ±60° | 弧 55px | ❌ 超 23px |
| dagger s2 | CAPSULE 长48/r12.8 | 束 60px | ✅ 基本对齐 |
| **sword s0** | RECT 64×25.6（盒心前移32） | **弧心前移32px** 60px | ✅ **全项目唯一对齐**（fx=px±32 恰好=盒心偏移） |
| sword s1 | SECTOR 64px ±120° | 弧 70px | ✅ 近对齐（装饰弧偏移无关判定） |
| sword s2 | CAPSULE 前伸 **112px** | shockwave 80px | ❌ 判定超视觉 32px |
| nunchaku s0 | CAPSULE 前伸 **134px** | 弧 90px | ❌ 超 44px |
| nunchaku s1 | CAPSULE 前伸 **144px** | 弧 95px | ❌ 超 49px |
| nunchaku s2 | CIRCLE 128px + 追踪 **192px** | pulse 32px | ❌❌ **判定超视觉 4-6 倍**；5 连击每击无命中 VFX |
| spear s0 | RECT 前伸 **224px**（含fudge） | 弧 72px | ❌❌ **超 152px（3.1×）**——"特效没碰到却掉血"主犯 |
| spear s1 | RECT 224px | 弧 72px | ❌❌ 同上 |
| spear s2 | SECTOR 192px ±30°×10击 | 仅弧 72px | ❌❌ 超 120px；且 shockwave 分支是死代码（特殊段 results 恒空） |
| crossbow s0-2 | 弹体最远 **840-1200px** | 弹体本体视觉 | ✅ **SSOT 成立**（见 §5） |

---

## 4. 异常点分级清单

### P1 — 视觉命中与伤害范围不一致（手感错位直接根源）

| # | 异常 | 修复方向预估 |
|---|------|------------|
| P1-1 | **spear s0/s1**：判定 224px vs 视觉 72px（weapons.json range 6 + hit_detection +16 fudge） | VFX 生成前伸 beam/矩形（复用 sword s0 盒心前移逻辑），radius 从 executor 传入 |
| P1-2 | **spear s2**：192px 扇形 ×10 击 vs 72px 弧；shockwave 在 results 空时不执行 | 特殊段无条件画 192px 范围指示（`range_indicator_timer` 通道现成） |
| P1-3 | **nunchaku s2**：CIRCLE 128 + 追踪 192 vs 32px pulse；5 连击零命中 VFX | tick_specials 每击注入 ring/beam |
| P1-4 | **dagger s0/s1**：视觉 50/55 > 判定 32px（"弧罩住怪但没伤害"） | 弧半径 = `rpx×1.1` 由 executor 传入，禁硬编码 |
| P1-5 | **crossbow**：玩家脚下 recipe VFX（零长 bolt/三圈 pulse）与飞行弹体零关联 | 删除脚下 recipe，弹体本体已足够；或小 muzzle flash |
| P1-6 | **weapons.json `vfx_recipe` 字段是死数据**（解析后全项目零消费者）；range 指示器 320px ≠ 实际射程 840px | 要么让 `_weapon_attack` 真读 `stage.vfx_recipe`，要么删字段 |

### P2 — 几何细节不一致

| # | 异常 | 修复方向 |
|---|------|---------|
| P2-1 | RECTANGLE 判定双轴硬编码 **+16px fudge**（盒膨胀 32px，**玩家身后 16px 也能被前向挥砍命中**） | 常量化为 MONSTER_HALF(14) 或改圆-矩形精确判定 |
| P2-2 | `_draw_slash_arc` 平分线恒偏 **30°**（DOWN 弧扫 0-120°，平分线 60° 应为 90°）——四种朝向视觉弧全偏 | `startAngle = facing_angle - 60` |
| P2-3 | crossbow s1 散布硬编码 ±15°，JSON width 15-18 全未读取 | `spread = stage.width` 接入 |
| P2-4 | sword s1/nunchaku s1 装饰弧偏移（+15,−10 / −20,+10 反向）与判定无空间对应 | 按朝向旋转偏移或移入纯表现层标记 |
| P2-5 | `HitShape::PROJECTILE` 分支是死代码（crossbow 在入口被拦截走弹体） | 删除枚举或 assert |
| P2-6 | FIST 双套数据并存：JSON fist_basic CIRCLE 32px 死配置 + legacy 48px 路径 | 统一或删死配置 |

### P3 — VFX 尺寸与 AoE 量级不一致

| # | 异常 |
|---|------|
| P3-1 | sword s2 shockwave 80 < 判定 112 |
| P3-2 | nunchaku s0/s1 弧 90/95 < 判定 134/144 |
| P3-3 | nunchaku 三套范围值互不相等（指示器 64-160 / 初判 128 / 追踪 192） |
| P3-4 | recipe `beam/bolt` 在 tx=0 且 target_dist=0（默认值）时退化为**零长线段**——所有 play_recipe 调用都传 0 |
| P3-5 | fist ring 28 vs CIRCLE 32（死分支，随 P2-6 处理） |
| P3-6 | 玩家挥砍前倾 4-7px/重击放大 1.25× 纯视觉（可接受，建议注释声明） |

---

## 5. 弓弩为什么正常（SSOT 参考实现）

弹体**一个字段 `pos` 驱动一切**：

```
发射: proj.pos = _player_origin()（碰撞盒中心）     weapon_executor.cpp:302
运动: p.pos.x += p.vel.x * dt                       :470
碰撞: CheckCollisionCircleRec(p.pos, 8, m->rect)    :481   ← 直接用 pos
渲染: DrawCircle(p.pos - cam, 6)                    game_scene.cpp:1957,1991
```

无中间缓存、无二次换算、无第二坐标系。碰撞半径 8 vs 视觉 6 的 2px 差是正常手感宽容度。**近战对齐的等价方案：让 slash/beam 类 Effect 携带 executor 的几何参数（origin/shape/range/width），渲染端按同一份数值画形状。**

---

## 6. G10.5-B 修复方案（待批准后实施）

**目标：不是重写 CombatSystem，而是让 VFX 消费判定层的真实几何。**

### 最小统一入口

`WeaponAttackResult` 已有 `hit_point`；扩展武器攻击上下文，让每段攻击把**判定用的 rpx/shape/width** 一并传出，`_weapon_attack` 的 VFX switch 从"拍脑袋像素"改为"executor 实参":

| 改动 | 内容 | 规模 |
|------|------|------|
| B-1 | `_weapon_attack` VFX 半径全部改用 `stage.range×TILE_SIZE` 派生（dagger 弧 50→35、spear 补 224px 前伸视觉、nunchaku 弧 90→134 等） | ~15 行 |
| B-2 | P2-2 slash_arc 30° 斜置修正 | 3 行 |
| B-3 | P2-1 RECTANGLE fudge +16 → MONSTER_HALF 常量 | 2 行 |
| B-4 | spear s2 / nunchaku s2 特殊段补范围指示 + 每击命中 VFX | ~10 行 |
| B-5 | P1-5/P3-4 crossbow 脚下 recipe 删除 / 零长 bolt 跳过 | ~5 行 |
| 不做 | vfx_recipe 字段接线（P1-6）、FIST 双套清理（P2-6）、Projectile 枚举删除（P2-5）——单开批次 | — |

### 冻结范围

❌ 不重写 hit_detection / 不动 CombatSystem / 不改 weapons.json 数值（只消费）/ 不做击退 / 不加新 VFX kind

---

## 7. Review Gate

- [ ] **D-A**: 对照表是否与你实机感受吻合（spear 是不是最离谱的？）
- [ ] **D-B**: B-1~B-5 修复范围同意？（P1-6/P2-5/P2-6 留后续批次）
- [ ] **D-C**: 修正后部分武器"可打击范围感觉变小"（视觉诚实地匹配判定）——是否接受？还是反过来放大判定值到视觉水平（改 JSON 数值，属平衡调整需 sim 验证）？
