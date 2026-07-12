#pragma once
#include <string>
#include <vector>
#include "raylib.h"

// ============================================================
// D6 Step5: PresentationSystemDirector — 统一表现层状态管理
// 组合模式: 持有 Shake/Freeze/DamageFloat/RoomMsg/Intro 所有状态
// GameScene 通过此 Director 访问表现层变量
// ============================================================

class PresentationSystemDirector {
public:
    // ── Damage Float ──
    struct DamageFloat { float x, y; float lifetime; int value; Color color; };
    std::vector<DamageFloat> damage_floats;
    void  spawn_damage(float wx, float wy, int dmg, Color c, float lifetime = 0.6f);

    // ── Camera Shake ──
    float shake_timer = 0.0f;
    float shake_intensity = 0.0f;
    void  trigger_shake(float intensity);

    // ── Frame Freeze ──
    float freeze_timer = 0.0f;
    void  trigger_freeze(float duration);

    // ── Room Message ──
    std::string room_msg;
    float room_msg_timer = 0.0f;
    void  show_message(const char* text, float duration = 2.5f);

    // ── Boss Intro Text ──
    std::string boss_intro_text;
    std::string boss_modifier_text;

    // ── Floor/Chapter Intro ──
    bool  floor_intro_active = false;
    float floor_intro_timer = 0.0f;
    float floor_intro_fade = 0.0f;
    int   floor_intro_floor = 0;
    bool  chapter_intro_active = false;
    float chapter_intro_timer = 0.0f;
    int   chapter_intro_ch = -1;

    // ── Debug panel flags (按F键切换) ──
    bool show_growth_debug = false;
    bool combat_juice_on = true;
    bool show_flow_debug = false;
    bool show_boss_behavior = false;
    bool show_boss_cmd = false;
    bool show_boss_report = false;
    int  last_combo_announced = 0;

    // ── Tick (更新所有表现层计时器) ──
    void tick(float dt);
};

// C1 helper
Color dmg_color_for(int dmg, bool is_magic, bool is_poison);
