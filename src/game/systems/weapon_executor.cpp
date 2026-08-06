#include "systems/weapon_executor.h"
#include "entities/player.h"
#include "entities/monster.h"
#include "systems/weapon_component.h"
#include "systems/combat_system.h"
#include "systems/hit_detection.h"
#include "systems/vfx_server.h"
#include "combat_feel.h"
#include "config.h"
#include "audio_server.h"
#include "core/event_bus.h"
#include "combat/element_resolver.h"  // G10.3
#include "ai/player_behavior/player_behavior_recorder.h" // F15.1
#include <algorithm>
#include <cmath>

// ── G9.3: map weapon type + stage to attack tag ──
static AttackTag _weapon_tag(WeaponType wt, int stage, bool is_stage3) {
    switch (wt) {
    case WeaponType::DAGGER:
        return is_stage3 ? AttackTag::PIERCE : AttackTag::SLASH;
    case WeaponType::SWORD:
        return (stage == 0 || is_stage3) ? AttackTag::BLUNT : AttackTag::SLASH;
    case WeaponType::NUNCHAKU:
        return is_stage3 ? AttackTag::MULTI_HIT : AttackTag::KNOCKBACK;
    case WeaponType::CROSSBOW:
        return is_stage3 ? AttackTag::MARKED : AttackTag::RANGED;
    case WeaponType::SPEAR:
        return is_stage3 ? AttackTag::PIERCE_STACK : AttackTag::PIERCE;
    default: return AttackTag::NONE;
    }
}

// ── G9.3: write AttackContext to Player for skill synergy ──
static void _set_attack_context(Player* p, float dmg, Monster* target,
                                  float game_time)
{
    const WeaponDef* def = p->weapon.current_def();
    if (!def) return;
    bool is_s3 = (p->weapon.combo_index() == 2 && def->stage_count >= 3);
    AttackTag tag = _weapon_tag(def->type, p->weapon.combo_index(), is_s3);
    AttackContext& ctx = p->last_attack;
    ctx.weapon_type = def->type;
    ctx.primary_tag = tag;
    ctx.combo_stage = p->weapon.combo_index();
    ctx.stage3_triggered = is_s3;
    ctx.damage_dealt = dmg;
    ctx.last_target = target;
    ctx.timestamp = (float)game_time;
}

// ── Helper: compute damage for a single hit ──
static int _calc_weapon_dmg(const Player* p, const Monster* target,
                             float stage_mult, bool& is_crit,
                             AttackType atype = AttackType::PHYSICAL)
{
    int atk = get_effective_attack(p);
    int def = target ? target->combat.get_effective_defense(atype) : 0;
    int base = calculate_damage(atk, def, atype);
    int dmg = (int)(base * stage_mult);
    int combo_idx = p->weapon.combo_index();
    float crit_chance = combo_idx >= 2 ? 0.30f : combo_idx == 1 ? 0.15f : 0.05f;
    if ((rng() % 1000) < (int)(crit_chance * 1000.0f)) { dmg *= 2; is_crit = true; }
    return std::max(1, dmg);
}

// ── Build raw target pointer list ──
static std::vector<Monster*> _raw_targets(
    const std::vector<Monster*>& targets)
{
    std::vector<Monster*> rt; rt.reserve(targets.size());
    for (auto* m : targets) if (m && m->combat.is_alive) rt.push_back(m);
    return rt;
}

// ── Get player origin ──
static Vector2 _player_origin(const Player* p) {
    return { p->entity.rect.x + p->entity.rect.width / 2,
             p->entity.rect.y + p->entity.rect.height / 2 };
}

// ── Resolve one hit into a result (damage + kill check) ──
static WeaponAttackResult _resolve_one(Player* p, Monster* m,
    const Vector2& hp, float mult, AttackType atype = AttackType::PHYSICAL)
{
    WeaponAttackResult ar;
    ar.target = m; ar.hit_point = hp; ar.is_crit = false;
    ar.damage = _calc_weapon_dmg(p, m, mult, ar.is_crit, atype);

    // G10.3: Element combat effects (fire crit / ice slow+freeze / poison DOT)
    bool did_freeze = false;
    ElementResolver::resolve(p, m, ar.damage, ar.is_crit, did_freeze);
    // Fire crit may increase damage + set is_crit → re-check if weapon crit was already set
    // ElementResolver already merged is_crit correctly

    int hp_before = m->combat.current_hp;
    m->combat.take_damage(ar.damage);
    ar.is_killing_blow = (!m->combat.is_alive && hp_before > 0);

    // G10.3: Element EXP
    if (ar.damage > 0) {
        ElementResolver::on_hit(p, m);
        if (ar.is_killing_blow) ElementResolver::on_kill(p, m);
    }
    return ar;
}

// ═══════════════════════════════════════════════════════════════
// WeaponSpecialState methods
// ═══════════════════════════════════════════════════════════════

void WeaponSpecialState::start(int max_h, float interval, float mult, float growth) {
    active = true; timer = 0.0f; hit_count = 0;
    max_hits = max_h; hit_interval = interval;
    next_hit_at = interval; base_mult = mult; mult_growth = growth;
}

bool WeaponSpecialState::should_fire_next(float dt) {
    if (!active) return false;
    timer += dt;
    if (timer >= next_hit_at && hit_count < max_hits) {
        next_hit_at += hit_interval; hit_count++;
        // Deactivate on the final hit, but keep hit_count/tracked so the
        // caller can still read this hit's multiplier and tracked target.
        if (hit_count >= max_hits) active = false;
        return true;
    }
    return false;
}

float WeaponSpecialState::current_multiplier() const {
    float m = base_mult;
    for (int i = 1; i < hit_count; ++i) m *= mult_growth;
    return m;
}

void WeaponSpecialState::reset() { active = false; timer = 0.0f; hit_count = 0; tracked = nullptr; }

// ═══════════════════════════════════════════════════════════════
// Forward decls for stage-3 special initiations
// ═══════════════════════════════════════════════════════════════
static bool _try_nunchaku_special(Player* p, const AttackStageDef& st,
    float rpx, float wp, const std::vector<Monster*>& rt);
static bool _try_spear_special(Player* p, const AttackStageDef& st, float rpx);
static bool _try_crossbow_power(Player* p, const AttackStageDef& st,
    Vector2 origin, std::vector<Projectile>* projs);
static void _crossbow_normal(Player* p, const AttackStageDef& st,
    Vector2 origin, int stage_idx, std::vector<Projectile>* projs);
static std::vector<WeaponAttackResult> _melee_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    Vector2 origin, float rpx, float wp, const std::vector<Monster*>& rt);

// ═══════════════════════════════════════════════════════════════
// WeaponExecutor — execute
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::execute(
    Player* player,
    const std::vector<Monster*>& targets,
    double game_time,
    AudioServer* audio,
    std::vector<Projectile>* projectiles)
{
    std::vector<WeaponAttackResult> results;
    if (!player || !player->combat.is_alive) return results;
    if (!player->weapon.can_attack(game_time)) return results;

    auto& w = player->weapon;
    const WeaponDef* def = w.current_def();
    if (!def) return results;

    const AttackStageDef& stage = w.current_stage();
    float rpx = stage.range * TILE_SIZE;
    float wp = (stage.hit_shape == HitShape::SECTOR) ? stage.width : stage.width * TILE_SIZE;
    Vector2 origin = _player_origin(player);
    auto rt = _raw_targets(targets);
    bool is_special = false;

    // ── Stage-3 special initiations ──
    if (w.combo_index() == 2 && def->stage_count >= 3) {
        if (def->type == WeaponType::NUNCHAKU)
            is_special = _try_nunchaku_special(player, stage, rpx, wp, rt);
        else if (def->type == WeaponType::SPEAR)
            is_special = _try_spear_special(player, stage, rpx);
        else if (def->type == WeaponType::CROSSBOW && projectiles)
            is_special = _try_crossbow_power(player, stage, origin, projectiles);
    }

    // ── G9.3: range indicator for ranged + nunchaku weapons ──
    if (def->type == WeaponType::SPEAR || def->type == WeaponType::CROSSBOW
        || def->type == WeaponType::NUNCHAKU) {
        w.range_indicator_timer = 0.25f;
        w.range_indicator_px = rpx;
    }

    // ── Normal resolution ──
    if (!is_special) {
        if (def->type == WeaponType::CROSSBOW && projectiles)
            _crossbow_normal(player, stage, origin, w.combo_index(), projectiles);
        else
            results = _melee_normal(player, def, stage, origin, rpx, wp, rt);
    }

    // ── G9.3: set attack context for skill synergy ──
    float total_dmg = 0.0f;
    Monster* prime_target = nullptr;
    for (auto& r : results) { total_dmg += r.damage; if (!prime_target) prime_target = r.target; }
    _set_attack_context(player, total_dmg, prime_target, (float)game_time);

    // ── Advance + emit + audio ──
    int stage_before = w.combo_index();
    w.execute_attack(game_time);
    GameEventType gev = stage_before == 0 ? GameEventType::WEAPON_STAGE_1
                      : stage_before == 1 ? GameEventType::WEAPON_STAGE_2
                      : GameEventType::WEAPON_SPECIAL;
    EventBus::inst().emit(gev, player, stage_before, stage.damage_multiplier,
        def->name.c_str());
    EventBus::inst().emit(GameEventType::WEAPON_ATTACK_COMPLETE, player,
        (int)stage.damage_multiplier * 100, total_dmg, def->name.c_str());
    // F15.2: record weapon usage with full context
    g_behavior.on_weapon_attack(weapon_type_name(def->type),
        (float)game_time, 0,  // floor set by game_scene
        player->entity.rect.x + player->entity.rect.width/2,
        player->entity.rect.y + player->entity.rect.height/2);
    if (audio) {
        const char* sfx = stage.sfx_name.empty() ? "melee" : stage.sfx_name.c_str();
        audio->play_sfx(sfx);
    }
    return results;
}

// ── Stage-3 special: nunchaku 5-hit auto-track flurry ──
static bool _try_nunchaku_special(Player* p, const AttackStageDef& st,
    float rpx, float wp, const std::vector<Monster*>& rt)
{
    Vector2 origin = _player_origin(p);
    auto hits = hit_detect_by_shape((int)st.hit_shape, origin,
        p->direction, rpx, wp, rt);
    void* tracked = hits.empty() ? nullptr : (void*)hits[0].target;
    auto& sp = p->weapon.runtime().special;
    const WeaponDef* def = p->weapon.current_def();
    int total_hits = 5;
    // Legendary: +2 hits (李小龙)
    if (def && def->legendary_effect == "nunchaku_hits") total_hits = 7;
    // Affix: damage_ramp — extra growth per hit
    float growth = 1.20f + (def ? def->affix.value : 0.0f);
    sp.start(total_hits, 0.08f, 0.80f, growth);
    sp.tracked = tracked;
    sp.range_px = rpx * 1.5f;
    sp.hit_shape = (int)HitShape::CIRCLE;
    sp.direction = p->direction;
    return true;
}

// ── Stage-3 special: spear 10-hit rapid pierce ──
static bool _try_spear_special(Player* p, const AttackStageDef& st, float rpx)
{
    auto& sp = p->weapon.runtime().special;
    const WeaponDef* def = p->weapon.current_def();
    int total_hits = 10;
    // Legendary: +2 hits (惊破天)
    if (def && def->legendary_effect == "spear_count") total_hits = 12;
    // Affix: pierce_bonus — extra multiplier per hit
    float mult = 1.10f + (def ? def->affix.value : 0.0f);
    sp.start(total_hits, 0.10f, mult, 1.0f);
    sp.range_px = rpx;
    sp.width_param = 30.0f;
    sp.hit_shape = (int)HitShape::SECTOR;
    sp.direction = p->direction;
    return true;
}

// ── Stage-3 special: crossbow power shot (piercing projectile + recoil) ──
static bool _try_crossbow_power(Player* p, const AttackStageDef& st,
    Vector2 origin, std::vector<Projectile>* projs)
{
    Vector2 fwd = {0, 1};
    switch (p->direction) {
    case Direction::UP: fwd = {0, -1}; break;
    case Direction::LEFT: fwd = {-1, 0}; break;
    case Direction::RIGHT: fwd = {1, 0}; break;
    default: break;
    }
    const WeaponDef* def = p->weapon.current_def();
    float legendary_bonus = (def && def->legendary_effect == "crossbow_power") ? 1.5f : 1.0f;
    Projectile proj;
    proj.pos = origin;
    proj.vel = { fwd.x * 800.0f, fwd.y * 800.0f };
    bool dummy_crit = false;
    proj.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier * legendary_bonus, dummy_crit);
    proj.piercing = true;
    proj.lifetime = 1.5f;
    proj.owner = (int)ProjectileOwner::PLAYER; projs->push_back(proj);
    p->entity.position.x -= fwd.x * TILE_SIZE;
    p->entity.position.y -= fwd.y * TILE_SIZE;
    p->entity.sync_rect();
    return true;
}

// ── Crossbow normal stages: fire projectiles ──
static void _crossbow_normal(Player* p, const AttackStageDef& st,
    Vector2 origin, int stage_idx, std::vector<Projectile>* projs)
{
    Vector2 fwd = {0, 1};
    switch (p->direction) {
    case Direction::UP: fwd = {0, -1}; break;
    case Direction::LEFT: fwd = {-1, 0}; break;
    case Direction::RIGHT: fwd = {1, 0}; break;
    default: break;
    }
    bool dc = false;
    if (stage_idx == 1) {
        for (float spread = -15.0f; spread <= 15.0f; spread += 15.0f) {
            float rad = spread * 3.14159f / 180.0f;
            float ca = cosf(rad), sa = sinf(rad);
            Projectile p2;
            p2.pos = origin;
            p2.vel = { (fwd.x * ca - fwd.y * sa) * 700.0f,
                       (fwd.x * sa + fwd.y * ca) * 700.0f };
            p2.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier, dc);
            p2.lifetime = 1.2f;
            projs->push_back(p2);
        }
    } else {
        Projectile proj;
        proj.pos = origin;
        proj.vel = { fwd.x * 700.0f, fwd.y * 700.0f };
        proj.damage = _calc_weapon_dmg(p, nullptr, st.damage_multiplier, dc);
        proj.lifetime = 1.2f;
        proj.owner = (int)ProjectileOwner::PLAYER; projs->push_back(proj);
    }
}

// ── Melee normal: instant hit detection + affix + legendary effects ──
static std::vector<WeaponAttackResult> _melee_normal(
    Player* p, const WeaponDef* def, const AttackStageDef& st,
    Vector2 origin, float rpx, float wp, const std::vector<Monster*>& rt)
{
    std::vector<WeaponAttackResult> results;
    // ── Affix: sword range_boost on stage-3 ──
    float effective_rpx = rpx;
    bool is_stage3 = (p->weapon.combo_index() == 2 && def->stage_count >= 3);
    if (is_stage3 && def->affix.type == "range_boost")
        effective_rpx *= (1.0f + def->affix.value);
    // ── Legendary: 倚天剑 wave expands range further ──
    if (is_stage3 && def->legendary_effect == "sword_wave")
        effective_rpx *= 1.3f;

    auto hits = hit_detect_by_shape(
        (int)st.hit_shape, origin, p->direction, effective_rpx, wp, rt);
    for (auto& h : hits) {
        float mult = st.damage_multiplier;
        // ── Affix: dagger bleed on stage-3 thrust ──
        bool apply_bleed = is_stage3 && def->affix.type == "bleed"
            && ((rng() % 100) < (int)def->affix.value);
        // ── Legendary: 恶魔之爪 always bleeds ──
        if (is_stage3 && def->legendary_effect == "dagger_bleed")
            apply_bleed = true;
        if (apply_bleed && h.target && h.target->combat.is_alive)
            apply_buff(h.target, "poison", 3);

        results.push_back(_resolve_one(p, h.target, h.hit_point, mult,
            (AttackType)st.damage_type));
    }
    // Sword stage-3 stun
    if (is_stage3 && def->type == WeaponType::SWORD)
        for (auto& h : hits)
            if (h.target && h.target->combat.is_alive)
                apply_buff(h.target, "slow", 3);
    return results;
}

// ═══════════════════════════════════════════════════════════════
// tick_specials — called each frame by GameScene
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::tick_specials(
    Player* player,
    const std::vector<Monster*>& targets,
    float dt)
{
    std::vector<WeaponAttackResult> results;
    if (!player) return results;

    auto& sp = player->weapon.runtime().special;
    if (!sp.active) return results;

    if (!sp.should_fire_next(dt)) return results;

    WeaponType wt = player->weapon.weapon_type();
    auto rt = _raw_targets(targets);
    Vector2 origin = _player_origin(player);
    float mult = sp.current_multiplier();

    if (wt == WeaponType::NUNCHAKU) {
        // Auto-track: hit the tracked target with auto-aim
        Monster* trg = (Monster*)sp.tracked;
        if (trg && trg->combat.is_alive) {
            Vector2 hp = { trg->entity.rect.x + trg->entity.rect.width / 2,
                           trg->entity.rect.y + trg->entity.rect.height / 2 };
            auto ar = _resolve_one(player, trg, hp, mult);
            ar.from_special = true;
            results.push_back(ar);
        } else if (!rt.empty()) {
            // Re-acquire nearest target
            auto hits = hit_detect_circle(origin, sp.range_px, rt);
            if (!hits.empty()) {
                sp.tracked = (void*)hits[0].target;
                auto ar = _resolve_one(player, hits[0].target, hits[0].hit_point, mult);
                ar.from_special = true;
                results.push_back(ar);
            }
        }
    }
    else if (wt == WeaponType::SPEAR) {
        // Sector rapid hits: magic-typed lightning-enhanced strikes
        auto hits = hit_detect_sector(origin, sp.direction,
            sp.range_px, sp.width_param, rt);
        for (auto& h : hits) {
            auto ar = _resolve_one(player, h.target, h.hit_point, mult,
                AttackType::MAGICAL);
            ar.from_special = true;
            results.push_back(ar);
        }
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════
// tick_projectiles — called each frame by GameScene
// ═══════════════════════════════════════════════════════════════

std::vector<WeaponAttackResult> WeaponExecutor::tick_projectiles(
    std::vector<Projectile>& projectiles,
    const std::vector<Monster*>& targets,
    float dt)
{
    std::vector<WeaponAttackResult> results;
    for (auto& p : projectiles) {
        if (!p.alive) continue;
        // D2: only tick PLAYER projectiles (MONSTER projs handled by game_scene)
        if (p.owner != (int)ProjectileOwner::PLAYER) continue;
        p.elapsed += dt;
        if (p.elapsed >= p.lifetime) { p.alive = false; continue; }
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        for (auto* m : targets) {
            if (!m || !m->combat.is_alive) continue;
            Rectangle mr = m->entity.rect;
            if (CheckCollisionCircleRec(p.pos, 8.0f, mr)) {
                WeaponAttackResult ar;
                ar.target = m; ar.hit_point = p.pos;
                ar.damage = p.damage; ar.is_crit = false;
                int hp_before = m->combat.current_hp;
                m->combat.take_damage(p.damage);
                ar.is_killing_blow = (!m->combat.is_alive && hp_before > 0);
                ar.from_special = true;
                results.push_back(ar);
                if (!p.piercing) { p.alive = false; break; }
            }
        }
    }
    // D2: cleanup handled centrally by game_scene after both PLAYER + MONSTER ticks
    return results;
}
