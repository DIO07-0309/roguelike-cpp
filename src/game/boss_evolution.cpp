#include "boss_evolution.h"
#include "relationship_system.h"

// ============================================================
// D5 Step2: Boss Evolution — 技能变体 + LastStand + Build反制
// ============================================================

// Build反制描述
const char* get_build_counter_desc(int boss_floor, BuildType bt) {
    switch (bt) {
        case BuildType::POISON_MASTER: return "周期性净化";
        case BuildType::FIRE_MAGE:     return boss_floor==10?"fire_resist":"火抗+";
        case BuildType::TIME_MASTER:   return "时间加速";
        case BuildType::BERSERKER:     return "格挡反击";
        case BuildType::SUPPORT:       return "治疗打断";
        default: return nullptr;
    }
}

// LastStand效果文字
static const char* last_stand_name(int floor) {
    if (floor == 5)  return "暗影狂怒";
    if (floor == 10) return "炼狱爆发";
    if (floor == 15) return "终焉降临";
    return "Last Stand";
}

BossEvolutionData calc_boss_evolution(int boss_floor,
    const WorldState& ws, BuildType bt,
    const RelationshipSystem& rels) {
    BossEvolutionData ev;
    ev.boss_floor = boss_floor;

    // ── Build反制 (6种BuildType) ──
    if (bt != BuildType::NONE) {
        int idx = (int)bt - 1;  // skip NONE=0
        if (idx >= 0 && idx < 6) ev.build_counter_active[idx] = true;
        switch (bt) {
        case BuildType::POISON_MASTER:
            // Boss周期净化(poison tick减半) + 冲锋伤害+
            ev.charge_mod.damage_scale = 1.15f; break;
        case BuildType::BERSERKER:
            // Boss格挡: charge cd延长 + shockwave范围小减
            ev.charge_mod.cooldown_scale = 0.85f;
            ev.shockwave_mod.area_scale = 0.90f; break;
        case BuildType::TIME_MASTER:
            // Boss时间加速: 全体CD-15%
            ev.charge_mod.cooldown_scale = 0.85f;
            ev.shockwave_mod.cooldown_scale = 0.85f;
            ev.summon_mod.cooldown_scale = 0.85f; break;
        case BuildType::FIRE_MAGE:
            // Boss火抗: HP倍率已在外层处理 (via calc_modifier)
            ev.shockwave_mod.damage_scale = 1.10f; break;
        default: break;
        }
    }

    // ── WorldFlag 技能变体 ──
    if (boss_floor == 5) {
        // F5 暗影骑士
        if (ws.has(WorldFlag::Blood_Ritual)) {
            ev.charge_mod.variant_name = "Blood Dash";
            ev.charge_mod.damage_scale += 0.15f;
        }
        if (ws.has(WorldFlag::Saved_Prisoner)) {
            ev.summon_mod.cooldown_scale = 1.30f;  // 召唤CD延长(囚犯祝福)
        }
        if (ws.has(WorldFlag::Accepted_Curse)) {
            ev.shockwave_mod.damage_scale += 0.10f;
        }
    } else if (boss_floor == 10) {
        // F10 地狱火魔
        if (ws.has(WorldFlag::Blood_Ritual)) {
            ev.shockwave_mod.variant_name = "Inferno";
            ev.shockwave_mod.area_scale += 0.25f;
            ev.shockwave_mod.damage_scale += 0.10f;
        }
        if (ws.has(WorldFlag::Saved_Priest)) {
            ev.charge_mod.damage_scale = 0.85f;  // 神官祝福: 冲锋减伤
            ev.evolution_name = "Blessed Flame";
        }
        if (ws.has(WorldFlag::Accepted_Curse)) {
            ev.summon_mod.extra_projectiles = 1;  // 多召1只
            ev.evolution_name = "Cursed Fire";
        }
    } else if (boss_floor == 15) {
        // F15 深渊之主
        if (ws.has(WorldFlag::Blood_Ritual)) {
            ev.charge_mod.variant_name = "Abyssal Charge";
            ev.charge_mod.damage_scale += 0.15f;
            ev.shockwave_mod.area_scale += 0.15f;
            ev.evolution_name = "Blood Avatar";
        }
        if (ws.has(WorldFlag::Boss1_Defeated) && ws.has(WorldFlag::Boss2_Defeated)) {
            ev.shockwave_mod.variant_name = "Tri-Pulse";
            ev.shockwave_mod.damage_scale += 0.12f;
        }
        if (ws.has(WorldFlag::Merchant_Killed)) {
            ev.summon_mod.double_strike = true;  // 召唤2x
            ev.evolution_name = ev.evolution_name ? ev.evolution_name : "Dark Overseer";
        }
        if (ws.has(WorldFlag::Saved_Priest)) {
            ev.charge_mod.cooldown_scale = 1.20f;  // CD延长
        }
        if (rels.check_relation(140, RelationType::RESPECT, 3)) {
            ev.shockwave_mod.damage_scale -= 0.10f;  // 守望者尊敬: 冲击波减伤
        }
        // Fear高: 额外弹幕
        bool high_fear = false;
        for (auto& r : rels.all()) { if (r.fear >= 4) { high_fear = true; break; } }
        if (high_fear) { ev.summon_mod.extra_projectiles += 1; }
    }

    // ── LastStand 名称 ──
    ev.evolution_name = ev.evolution_name ? ev.evolution_name : last_stand_name(boss_floor);

    // Clamp sanity
    if (ev.charge_mod.cooldown_scale < 0.60f)     ev.charge_mod.cooldown_scale = 0.60f;
    if (ev.shockwave_mod.cooldown_scale < 0.60f)  ev.shockwave_mod.cooldown_scale = 0.60f;
    if (ev.summon_mod.cooldown_scale < 0.60f)     ev.summon_mod.cooldown_scale = 0.60f;
    if (ev.charge_mod.damage_scale > 1.50f)       ev.charge_mod.damage_scale = 1.50f;
    if (ev.shockwave_mod.damage_scale > 1.50f)    ev.shockwave_mod.damage_scale = 1.50f;

    return ev;
}
