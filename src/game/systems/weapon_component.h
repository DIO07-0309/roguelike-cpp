#pragma once
#include <string>
#include "types/weapon_types.h"

// ============================================================
// G9: WeaponRuntime — per-attack state (NOT serialized to JSON)
// Attached to Player, driven by weapon data
// ============================================================
struct WeaponRuntime {
    int combo_index = 0;         // 0→1→2→0 (or 0 only for fist)
    float combo_timer = 0.0f;   // decays each frame, timeout → reset
    float recovery_timer = 0.0f; // remaining recovery after attack
    bool is_attacking = false;   // true during windup + hit_frame + recovery
    bool in_fatigue = false;     // crossbow stage-3 fatigue
    float fatigue_timer = 0.0f;
    float last_attack_time = -999.0f;
    std::string current_weapon_id; // active weapon def id

    // G9.1: multi-hit special state
    WeaponSpecialState special;

    static constexpr float DEFAULT_COMBO_TIMEOUT = 0.80f;

    void tick(float dt, float combo_timeout = DEFAULT_COMBO_TIMEOUT);
    void start_attack(const AttackStageDef& stage);
    void advance(float combo_timeout = DEFAULT_COMBO_TIMEOUT);
    void reset_combo();
    bool can_act() const;
};

// ============================================================
// G9: WeaponComponent — composited into Player
// Single responsibility: manage weapon state + delegate to WeaponDef
// ============================================================
class WeaponComponent {
public:
    WeaponComponent();

    // ── Equip / Unequip ──
    void equip(const std::string& weapon_id);
    void unequip();
    const WeaponDef* current_def() const { return _def; }
    const char* current_weapon_id() const { return _runtime.current_weapon_id.c_str(); }
    WeaponType weapon_type() const;

    // ── Runtime state queries ──
    bool can_attack(double game_time) const;
    int  combo_index() const { return _runtime.combo_index; }
    bool is_in_fatigue() const { return _runtime.in_fatigue; }
    bool is_attacking() const { return _runtime.is_attacking; }

    // ── Action ──
    void execute_attack(double game_time); // advance combo + set recovery
    void tick(float dt);

    // ── Current stage data (read from WeaponDef) ──
    const AttackStageDef& current_stage() const;

    // ── Backward-compat shims (match old ComboState interface) ──
    float multiplier() const;
    bool  is_heavy() const;
    void  hit(double game_time);    // same signature as ComboState::hit()
    float get_combo_timer() const { return _runtime.combo_timer; }

    // ── Hit shape info for WeaponExecutor ──
    HitShape current_hit_shape() const;
    float   current_range() const;    // in tiles
    float   current_width() const;    // secondary dimension
    float   current_shake() const;
    const char* current_sfx() const;

    // ── G9.3: access runtime special state from WeaponExecutor ──
    WeaponRuntime& runtime() { return _runtime; }
    const WeaponRuntime& runtime() const { return _runtime; }

    // ── G9.3: range indicator timer (visible briefly after attack) ──
    float range_indicator_timer = 0.0f;
    float range_indicator_px = 0.0f;

private:
    WeaponRuntime _runtime;
    const WeaponDef* _def = nullptr; // points to loaded registry entry

    const WeaponDef* _get_or_fist() const; // fallback to fist_basic
};
