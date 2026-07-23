#include "presentation_system_director.h"
#include "event_bus.h"
#include "service_locator.h"   // G1: access presentation from static callback
#include "skill_evolution.h"   // G1 Step3
#include "rule_chain.h"         // G1 Step4: rule_display_name
#include "player.h"            // G1 Step3: access skills
#include "vfx_server.h"        // G5.8: unified VFX entry
#include "build_score.h"       // G5.8: build theme colors
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

Color dmg_color_for(int dmg, bool is_magic, bool is_poison) {
    // G5.8.2: build theme tints damage numbers
    if (is_poison) return {40, 220, 80, 255};
    if (is_magic)  return {160, 120, 255, 255};
    if (dmg >= 50) return {255, 220, 50, 255};
    auto* pres = ServiceLocator::get<PresentationSystemDirector>();
    if (pres && dmg >= 25) {
        // large hits use theme secondary color
        Color sc = pres->get_theme().secondary;
        return Color{sc.r, sc.g, sc.b, 240};
    }
    // small hits: slight accent tint
    if (pres && dmg >= 10) {
        Color ac = pres->get_theme().accent;
        return Color{(unsigned char)((200 + ac.r) / 2),
                     (unsigned char)((200 + ac.g) / 2),
                     (unsigned char)((200 + ac.b) / 2), 220};
    }
    return {220, 220, 220, 220};
}

// ============================================================
// PresentationSystemDirector 实现
// ============================================================

void PresentationSystemDirector::spawn_damage(float wx, float wy, int dmg, Color c, float lt) {
    damage_floats.push_back({wx, wy, lt, dmg, c});
}

void PresentationSystemDirector::trigger_shake(float intensity) {
    if (intensity > shake_intensity) shake_intensity = intensity;
    shake_timer = 0.12f;
}

void PresentationSystemDirector::trigger_freeze(float duration) {
    freeze_timer = duration;
}

void PresentationSystemDirector::show_message(const char* text, float duration) {
    room_msg = text;
    room_msg_timer = duration;
}

// ═══════════════════════════════════════════════════════════════
// G5.8.2: BuildTheme — 12 full theme presets
// ═══════════════════════════════════════════════════════════════

// Helper: fill theme sub-structs consistently
static void _fill_theme(BuildTheme& t,
    Color p, Color s, Color a, const char* n, const char* vp,
    float ps, float es, float bw, float rs, float ss, float ls,
    float cs, float fb, float zo, float da,
    Color et, Color hf, float vg,
    const char* hs, const char* ssx, float psh, float rv) {
    t.primary=p; t.secondary=s; t.accent=a; t.name=n; t.vfx_preset=vp;
    t.vfx.particle_speed=ps; t.vfx.explosion_scale=es; t.vfx.beam_width=bw;
    t.vfx.ring_speed=rs; t.vfx.spark_scale=ss; t.vfx.lifetime_scale=ls;
    t.camera.shake_scale=cs; t.camera.freeze_bonus=fb; t.camera.zoom_offset=zo; t.camera.dash_offset=da;
    t.screen.edge_tint=et; t.screen.hit_flash_tint=hf; t.screen.vignette_strength=vg;
    t.audio.hit_sfx=hs; t.audio.skill_sfx=ssx; t.audio.pitch_shift=psh; t.audio.reverb_mix=rv;
}

BuildTheme BuildTheme::from_build_type(BuildType bt) {
    BuildTheme t;
    switch (bt) {
    case BuildType::ICE_MAGE:
        _fill_theme(t,{100,200,255,255},{160,230,255,220},{80,180,240,200},"ice","ice",
            0.70f,1.15f,1.0f,1.0f,1.0f,1.05f, 0.80f,0.02f,-0.03f,0.0f,
            {100,180,240,40},{180,220,255,200},0.12f, "slash","bolt",0.05f,0.0f); break;
    case BuildType::FIRE_MAGE:
        _fill_theme(t,{255,140,30,255},{255,100,20,220},{240,80,15,200},"fire","fire",
            1.30f,1.25f,1.1f,0.9f,1.0f,0.90f, 1.30f,0.0f,0.03f,8.0f,
            {255,100,20,30},{255,200,100,200},0.0f, "melee","bolt",-0.10f,0.0f); break;
    case BuildType::POISON_MASTER:
        _fill_theme(t,{80,220,80,255},{40,200,60,220},{30,180,50,200},"nature","nature",
            1.00f,1.00f,1.0f,1.0f,1.0f,1.05f, 1.00f,0.0f,0.0f,0.0f,
            {30,180,50,30},{40,220,80,180},0.0f, "slash","bolt",0.0f,0.0f); break;
    case BuildType::LIGHTNING_MAGE:
        _fill_theme(t,{220,220,80,255},{240,240,120,220},{200,200,60,200},"lightning","lightning",
            1.40f,1.10f,1.2f,0.85f,1.1f,0.85f, 1.15f,0.0f,0.0f,12.0f,
            {200,200,60,20},{255,240,120,180},0.0f, "bolt","bolt",0.10f,0.10f); break;
    case BuildType::BERSERKER:
        _fill_theme(t,{255,80,50,255},{240,60,30,220},{220,40,20,200},"blood","blood",
            1.10f,1.05f,1.0f,0.95f,1.0f,0.90f, 1.20f,0.0f,0.02f,6.0f,
            {220,40,20,20},{255,100,60,180},0.0f, "melee","melee",-0.05f,0.0f); break;
    case BuildType::BLEED_BLADE:
        _fill_theme(t,{200,30,60,255},{180,20,40,220},{160,15,30,200},"blood","blood",
            1.05f,1.10f,1.0f,1.0f,1.05f,0.95f, 1.10f,0.0f,0.01f,4.0f,
            {160,15,30,25},{200,30,60,180},0.08f, "melee","slash",-0.05f,0.0f); break;
    case BuildType::SHADOW_STRIKER:
        _fill_theme(t,{140,40,220,255},{120,30,200,220},{100,20,180,200},"shadow","shadow",
            0.85f,0.90f,0.9f,1.1f,0.85f,1.15f, 0.70f,0.04f,-0.05f,14.0f,
            {80,20,140,50},{120,40,200,160},0.25f, "slash","bolt",-0.15f,0.30f); break;
    case BuildType::TIME_MASTER:
        _fill_theme(t,{180,200,240,255},{160,190,230,220},{140,180,220,200},"ice","ice",
            0.90f,1.00f,1.0f,1.05f,1.0f,1.10f, 0.90f,0.06f,0.0f,0.0f,
            {140,180,220,30},{180,200,240,160},0.10f, "slash","bolt",0.08f,0.15f); break;
    case BuildType::SUPPORT:
        _fill_theme(t,{100,240,120,255},{80,220,100,220},{60,200,80,200},"nature","nature",
            1.00f,0.95f,1.0f,1.0f,1.0f,1.05f, 0.85f,0.0f,0.0f,0.0f,
            {60,200,80,20},{100,240,120,160},0.0f, "slash","bolt",0.0f,0.0f); break;
    case BuildType::JUGGERNAUT:
        _fill_theme(t,{180,180,200,255},{160,160,190,220},{140,140,180,200},"white","white",
            0.85f,0.90f,1.1f,0.90f,0.90f,1.10f, 0.75f,0.0f,0.0f,0.0f,
            {140,140,180,30},{180,180,200,160},0.05f, "melee","melee",-0.10f,0.0f); break;
    case BuildType::SUMMON_LORD:
        _fill_theme(t,{100,200,200,255},{80,180,180,220},{60,160,160,200},"ice","ice",
            0.95f,1.00f,1.0f,1.0f,1.05f,1.05f, 1.00f,0.0f,0.0f,0.0f,
            {60,160,160,25},{100,200,200,170},0.05f, "bolt","bolt",0.0f,0.10f); break;
    case BuildType::PROJECTILE:
        _fill_theme(t,{255,200,80,255},{240,180,60,220},{220,160,50,200},"gold","gold",
            1.15f,1.05f,1.0f,0.95f,1.0f,0.95f, 1.05f,0.0f,0.0f,5.0f,
            {220,160,50,20},{255,200,80,180},0.0f, "bolt","bolt",0.0f,0.0f); break;
    default:
        _fill_theme(t,{255,200,50,255},{240,180,40,220},{220,160,30,200},"gold","",
            1.0f,1.0f,1.0f,1.0f,1.0f,1.0f, 1.0f,0.0f,0.0f,0.0f,
            {0,0,0,0},{255,255,255,200},0.0f, "melee","bolt",0.0f,0.0f); break;
    }
    return t;
}

void PresentationSystemDirector::set_build_theme(BuildType bt) {
    _theme = BuildTheme::from_build_type(bt);
}

// get_build_color() is defined inline in the header

// ═══════════════════════════════════════════════════════════════
// G5.8: Unified VFX dispatch — Gameplay never touches VFXServer directly
// ═══════════════════════════════════════════════════════════════

void PresentationSystemDirector::emit_skill_vfx(VFXServer& vfx, const char* skill_id,
    float cx, float cy, int level, Direction dir, float tx, float ty, int extra) {
    std::string sid(skill_id);

    if (sid == "slash" || sid == "frost_edge" || sid == "blood_slash")
        vfx.slash_skill(cx, cy, dir, level);
    else if (sid == "fireball" || sid == "meteor")
        vfx.fireball(cx, cy, tx, ty, level);
    else if (sid == "self_heal" || sid == "blood_pact" || sid == "blizzard_ward")
        vfx.heal(cx, cy, level);
    else if (sid == "the_world" || sid == "shadow_walk" || sid == "lightning_dash")
        vfx.time_stop(cx, cy);
    else if (sid == "ice_nova")
        vfx.ice_nova(cx, cy, 120.0f + level * 20.0f, level);
    else if (sid == "chain_lightning")
        vfx.chain_lightning(cx, cy, tx, ty, extra); // extra = bounces
    else if (sid == "shadow_strike")
        vfx.shadow_strike(cx, cy, tx, ty, level);
    else if (sid == "blood_frenzy")
        vfx.blood_frenzy(cx, cy, 120.0f + level * 20.0f, extra); // extra = hit_count
    else if (sid == "summon_spirit")
        vfx.summon_spirit(tx, ty, extra); // tx,ty = spawn pos, extra = count
    else
        vfx.slash_skill(cx, cy, dir, level); // fallback
}

void PresentationSystemDirector::emit_archetype_vfx(VFXServer& vfx, const char* archetype,
    float cx, float cy, float tx, float ty) {
    std::string a(archetype);
    if (a == "sniper")
        vfx.sniper_line(cx, cy, tx, ty);
    else if (a == "controller")
        vfx.controller_zone(tx, ty, 60.0f);
    else if (a == "ambush")
        vfx.ambush_smoke(cx, cy);
    else if (a == "guardian")
        vfx.guardian_aura_enemy(cx, cy, 80.0f);
}

void PresentationSystemDirector::emit_boss_phase2_vfx(VFXServer& vfx, const char* boss_id,
    float bx, float by, Color tint, float px, float py) {
    std::string bid(boss_id);
    vfx.boss_phase2_flash(bx, by, tint);
    if (bid == "demon_lord")
        vfx.boss_gravity_pull(bx, by, px, py);
}

void PresentationSystemDirector::tick(float dt) {
    // Shake decay
    if (shake_timer > 0) shake_timer -= dt;
    // Freeze decay
    if (freeze_timer > 0) { freeze_timer -= dt; if (freeze_timer <= 0) freeze_timer = 0; }
    // Room message
    if (room_msg_timer > 0) room_msg_timer -= dt;

    // Damage floats decay + cleanup
    for (auto& df : damage_floats) df.lifetime -= dt;
    damage_floats.erase(
        std::remove_if(damage_floats.begin(), damage_floats.end(),
            [](auto& df) { return df.lifetime <= 0; }),
        damage_floats.end());

    // Floor intro
    if (floor_intro_active) {
        floor_intro_timer -= dt;
        floor_intro_fade = (floor_intro_fade < 1.0f)
            ? std::min(1.0f, floor_intro_fade + dt * 2.5f) : 1.0f;
        if (floor_intro_timer <= 0) floor_intro_active = false;
    }
    // Chapter intro
    if (chapter_intro_active) {
        chapter_intro_timer -= dt;
        if (chapter_intro_timer <= 0) chapter_intro_active = false;
    }
}

void PresentationSystemDirector::init_events() {
    EventBus::inst().subscribe(GameEventType::PLAYER_LEVEL_UP,
        [](const GameEvent& ev) {
            (void)ev;  // reserved: 升级特效
        }, "Presentation");
    EventBus::inst().subscribe(GameEventType::MONSTER_DIED,
        [](const GameEvent& ev) {
            (void)ev;  // reserved: 击杀飘字/震动
        }, "Presentation");
    // G1: 普攻进化 — 弹 toast 消息
    EventBus::inst().subscribe(GameEventType::ATTACK_EVOLVED,
        [](const GameEvent& ev) {
            int new_lv = ev.int_val & 0xFFFF;
            const char* name = (new_lv == 3) ? "旋风斩" : "剑气";
            // PresentationSystemDirector 需要在 GameScene 上下文中才能调用 show_message
            // 这里通过 ServiceLocator 获取实例
            auto* pres = ServiceLocator::get<PresentationSystemDirector>();
            if (pres && new_lv >= 2) {
                char buf[64];
                snprintf(buf, sizeof(buf), "普攻进化: %s (Lv%d)", name, new_lv);
                pres->show_message(buf, 2.5f);
            }
        }, "Presentation");
    // G1 Step3: 技能进化 — 弹 toast
    EventBus::inst().subscribe(GameEventType::SKILL_EVOLVED,
        [](const GameEvent& ev) {
            int new_lv = SkillEvolutionManager::event_new_level(ev.int_val);
            int sk_idx = SkillEvolutionManager::event_skill_index(ev.int_val);
            auto* pres = ServiceLocator::get<PresentationSystemDirector>();
            if (pres && ev.sender && new_lv >= 1) {
                auto* player = static_cast<Player*>(ev.sender);
                if (sk_idx >= 0 && sk_idx < (int)player->skills.active_skills.size()) {
                    auto& sk = player->skills.active_skills[sk_idx];
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%s 进化: %s",
                             sk->name.c_str(), sk->get_evolution_text().c_str());
                    pres->show_message(buf, 2.5f);
                }
            }
        }, "Presentation");
    // G1 Step4: Boss 规则激活 — 弹 toast
    EventBus::inst().subscribe(GameEventType::BOSS_RULE_ACTIVATED,
        [](const GameEvent& ev) {
            auto* pres = ServiceLocator::get<PresentationSystemDirector>();
            if (pres && ev.str_val) {
                const char* name = RuleChainManager::rule_display_name(ev.str_val);
                char buf[80];
                snprintf(buf, sizeof(buf), "Boss 规则激活: %s", name);
                pres->show_message(buf, 3.0f);
            }
        }, "Presentation");
    // G2.4: Quest 完成 — 弹 toast
    EventBus::inst().subscribe(GameEventType::QUEST_COMPLETE,
        [](const GameEvent& ev) {
            auto* pres = ServiceLocator::get<PresentationSystemDirector>();
            if (pres && ev.str_val) {
                char buf[80];
                snprintf(buf, sizeof(buf), "任务完成: %s", ev.str_val);
                pres->show_message(buf, 2.5f);
            }
        }, "Presentation");
}
