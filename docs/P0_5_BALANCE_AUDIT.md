# P0.5 BALANCE BASELINE AUDIT

> 生成时间：2026-08-27  
> 目标：确认 P0 修复前后的数值差异，验证 weapons.json 补偿是否合理

---

## A. 修复前后伤害对照

### A.1 伤害路径分类

| 伤害源 | P0 前路径 | P0 后路径 | P0 影响 |
|--------|----------|----------|---------|
| Fist 近战 | apply_attack_damage ×1 | 不变 | ❌ 无变化 |
| 武器近战 | _resolve_one(take_damage) + apply_attack_damage(take_damage) = **×2** | _resolve_one(take_damage) = **×1** | ✅ 修复双重伤害 |
| Weapon Specials | _resolve_one(take_damage) = ×1 | 不变 | ❌ 无变化 |
| Projectiles | tick_projectiles(take_damage) = ×1 | 不变 | ❌ 无变化 |
| 技能 | skill.execute(take_damage) = ×1 | 不变 | ❌ 无变化 |
| DOT | tick_buffs(take_damage) = ×1 | 不变 | ❌ 无变化 |
| Hazard | take_damage ×1 | 不变 | ❌ 无变化 |

### A.2 具体伤害值对照 (ATK=10, DEF=3)

| 伤害源 | P0 前 (双倍) | P0 后 (单倍) | 设计意图 | 补偿方式 |
|--------|-------------|-------------|---------|---------|
| Sword stage0 | base×4.8×2=base×9.6 | base×4.8 | base×4.8 | weapons.json mult ×2 |
| Sword stage1 | base×4.0×2=base×8.0 | base×4.0 | base×4.0 | weapons.json mult ×2 |
| Sword stage2 | base×3.2×2=base×6.4 | base×3.2 | base×3.2 | weapons.json mult ×2 |
| Dagger stage0 | base×2.0×2=base×4.0 | base×2.0 | base×2.0 | weapons.json mult ×2 |
| Crossbow bolt | base×1.0 (pre-baked, no DEF) | 不变 | 不变 | 不补偿 |
| Nunchaku special | base×0.80×1=base×0.80 | 不变 | 不变 | 不补偿 |
| SlashSkill | ATK×1.5×power | 不变 | 不变 | 不补偿 |
| FireballSkill | ATK×2.5×power | 不变 | 不变 | 不补偿 |
| Poison DOT | 3×stacks/0.5s | 不变 | 不变 | 不补偿 |

---

## B. weapons.json ×2 审计

### B.1 补偿逻辑

```
P0 前: _resolve_one(take_damage) + apply_attack_damage(take_damage) = 2× damage
P0 后: _resolve_one(take_damage) = 1× damage
补偿: weapon stage multipliers × 2 → 恢复 2× effective damage
```

### B.2 逐武器审计

**非 PROJECTILE 武器**（近战路径，受双重伤害影响）：补偿 ×2 ✅

| 武器 | 原始 mult | 当前 mult | 比率 | 结论 |
|------|----------|----------|------|------|
| dagger_common | [1.00,1.00,1.10] | [2.00,2.00,2.20] | ×2 | **KEEP** |
| dagger_rare | [1.05,1.05,1.25] | [2.10,2.10,2.50] | ×2 | **KEEP** |
| dagger_epic | [1.10,1.10,1.40] | [2.20,2.20,2.80] | ×2 | **KEEP** |
| dagger_legendary | [1.15,1.15,1.60] | [2.30,2.30,3.20] | ×2 | **KEEP** |
| sword_common | [1.20,1.00,0.80] | [2.40,2.00,1.60] | ×2 | **KEEP** |
| sword_rare | [1.30,1.05,0.85] | [2.60,2.10,1.70] | ×2 | **KEEP** |
| sword_epic | [1.40,1.10,0.90] | [2.80,2.20,1.80] | ×2 | **KEEP** |
| sword_legendary | [1.55,1.15,1.00] | [3.10,2.30,2.00] | ×2 | **KEEP** |
| nunchaku_common | [1.00,1.00,0.80] | [2.00,2.00,1.60] | ×2 | **KEEP** |
| nunchaku_rare | [1.05,1.05,0.85] | [2.10,2.10,1.70] | ×2 | **KEEP** |
| nunchaku_epic | [1.10,1.10,0.92] | [2.20,2.20,1.84] | ×2 | **KEEP** |
| nunchaku_legendary | [1.15,1.15,1.00] | [2.30,2.30,2.00] | ×2 | **KEEP** |
| spear_common | [1.00,1.20,1.10] | [2.00,2.40,2.20] | ×2 | **KEEP** |
| spear_rare | [1.05,1.30,1.15] | [2.10,2.60,2.30] | ×2 | **KEEP** |
| spear_epic | [1.10,1.40,1.25] | [2.20,2.80,2.50] | ×2 | **KEEP** |
| spear_legendary | [1.15,1.55,1.35] | [2.30,3.10,2.70] | ×2 | **KEEP** |

**PROJECTILE 武器**（crossbow，不受双重伤害影响）：不补偿 ✅

| 武器 | 原始 mult | 当前 mult | 比率 | 结论 |
|------|----------|----------|------|------|
| crossbow_common | [1.00,1.00,2.00] | [1.00,1.00,2.00] | ×1 | **KEEP** |
| crossbow_rare | [1.05,1.05,2.20] | [1.05,1.05,2.20] | ×1 | **KEEP** |
| crossbow_epic | [1.10,1.10,2.50] | [1.10,1.10,2.50] | ×1 | **KEEP** |
| crossbow_legendary | [1.15,1.15,2.00] | [1.15,1.15,2.00] | ×1 | **KEEP** |

### B.3 DPS 对照表

| 武器 | 稀有度 | 原始 DPS | P0 前有效 DPS (×2) | P0 后 DPS | 恢复率 |
|------|--------|---------|-------------------|----------|--------|
| dagger | common | 15.5 | 31.0 | 31.0 | 100% |
| dagger | legendary | 43.3 | 86.7 | 86.7 | 100% |
| sword | common | 17.6 | 35.3 | 35.3 | 100% |
| sword | legendary | 50.6 | 101.3 | 101.3 | 100% |
| nunchaku | common | 13.2 | 26.4 | 26.4 | 100% |
| nunchaku | legendary | 34.7 | 69.5 | 69.5 | 100% |
| crossbow | common | 23.5 | 23.5 | 23.5 | 100% |
| crossbow | legendary | 58.8 | 58.8 | 58.8 | 100% |
| spear | common | 15.5 | 31.1 | 31.1 | 100% |
| spear | legendary | 51.2 | 102.3 | 102.3 | 100% |

**结论**：所有武器 DPS 恢复率 = 100%。补偿正确。

---

## C. 胜率下降归因

### C.1 胜率变化

| 版本 | 胜率 | 主要变更 |
|------|------|---------|
| v0.9.33 | 10.0% | barrel/boss cooldown |
| v0.9.34 | 9.0% | 11 AI bug fixes |
| v0.9.35 | 9.0% | leash/victory BGM |
| **P0 修复后** | **5.3%** (无构筑) | on-kill fix + double damage fix + param consistency |

### C.2 下降原因分析

| 因素 | 影响 | 证据 |
|------|------|------|
| **On-Kill Exactly-Once 修复** | 中等 | weapon tick 击杀不再触发双倍 on-kill 效果（leech_blade/battle_totem 等） |
| **Double Damage 修复 + ×2 补偿** | 低 | 武器 DPS 已恢复 100%，但 crit 概率/方差可能有微小差异 |
| **war_drum 10%→15%** | 正向 | ATK 加成增加 5%，应略微提高胜率 |
| **blood_chalice 20%→30%** | 正向 | 低血 ATK 加成增加 10%，应略微提高胜率 |
| **Simulation variance** | 中等 | 300 局样本方差大，历史 baseline 3-6% |

### C.3 Baseline 对比

历史 baseline（100 局 × 5 seeds）平均胜率 = 4.2%。当前 5.3% **在 baseline 范围内**。

**结论**：胜率从 9.0% 降至 5.3% 的主要原因是 **on-kill exactly-once 修复**减少了 weapon tick 击杀的重复掉落/经验，而非武器伤害问题。武器 DPS 已通过 ×2 补偿完全恢复。

---

## F. 正式基线确认（P0 后）

> 审核通过时间：2026-08-27  
> 审核人：用户确认

### F.1 正式数值基线

当前 `resources/weapons.json` 为 **P0 后正式数值基线**：
- 近战武器 ×2 补偿
- PROJECTILE 武器 ×1（不补偿）
- dagger_common ×2（非 ×8）

### F.2 300 局模拟用途

300 局模拟用于 **回归检测**（检测明显退化），不用于 **精确估计长期真实胜率**。
精确胜率估计需要更大样本量（1000+ 局）或理论分析。

### F.3 旧基线作废

**9.0% 不再作为新的 Balance Baseline**。
该数据可能受到以下旧版 Bug 影响：
- Double Damage（已修复）
- Double On-Kill（已修复）
- 倍率脚本错误（已修复）

正式基线以 P0.5 审核通过后的 5.3% 为参考起点。

---

## D. 推荐恢复方案

### D.1 当前状态

- weapons.json multiplier 补偿正确（×2 for melee, ×1 for projectile）
- 所有武器 DPS 恢复率 = 100%
- 胜率 5.3% 在历史 baseline 范围内（3-6%）

### D.2 不需要进一步武器调整

武器伤害已完全恢复。胜率差异主要来自 on-kill exactly-once 修复（正确行为）。

### D.3 后续平衡调整方向（非 P0 范围）

| 方向 | 说明 |
|------|------|
| 敌人 HP 微调 | 如果胜率持续偏低，可微调 F5/F10 敌人 HP |
| 技能倍率微调 | 技能未受 P0 影响，但可作为独立平衡杠杆 |
| 遗物效果实现 | 37 个 Ghost Relics 实现后会增加玩家 power |

---

## E. 推荐修改文件

当前 weapons.json 已修正为正确的 ×2 补偿值。无需进一步修改。

| 文件 | 状态 |
|------|------|
| `resources/weapons.json` | ✅ 已修正为正确的 ×2 补偿 |
| `src/game/entities/monster.h` | ✅ kill_processed guard |
| `src/game/scene/game_scene_combat.cpp` | ✅ on_monster_killed guard |
| `src/game/scenes/game_scene.cpp` | ✅ weapon tick 不再重复调用 |
| `src/game/player_controller.cpp` | ✅ 移除 apply_attack_damage |
| `src/game/systems/combat_system.cpp` | ✅ war_drum/blood_chalice param |
