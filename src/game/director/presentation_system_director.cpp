#include "presentation_system_director.h"
#include "event_bus.h"
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
}
