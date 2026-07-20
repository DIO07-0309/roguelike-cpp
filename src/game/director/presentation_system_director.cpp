#include "presentation_system_director.h"
#include "event_bus.h"
#include "service_locator.h"   // G1: access presentation from static callback
#include "skill_evolution.h"   // G1 Step3
#include "rule_chain.h"         // G1 Step4: rule_display_name
#include "player.h"            // G1 Step3: access skills
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

Color dmg_color_for(int dmg, bool is_magic, bool is_poison) {
    if (is_poison) return {40, 220, 80, 255};
    if (is_magic)  return {160, 120, 255, 255};
    if (dmg >= 50) return {255, 220, 50, 255};
    return {255, 255, 255, 255};
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
