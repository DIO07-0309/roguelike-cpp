#include "systems/weapon_component.h"
#include "data/weapon_defs.h"
#include <algorithm>

// ── WeaponRuntime ──

void WeaponRuntime::tick(float dt, float combo_timeout) {
    // Decay combo timer
    if (combo_timer > 0.0f) {
        combo_timer -= dt;
        if (combo_timer <= 0.0f) {
            combo_timer = 0.0f;
            combo_index = 0; // timeout → reset
        }
    }
    // Decay recovery
    if (recovery_timer > 0.0f) {
        recovery_timer -= dt;
        if (recovery_timer <= 0.0f) {
            recovery_timer = 0.0f;
            is_attacking = false;
        }
    }
    // Decay fatigue
    if (in_fatigue && fatigue_timer > 0.0f) {
        fatigue_timer -= dt;
        if (fatigue_timer <= 0.0f) {
            fatigue_timer = 0.0f;
            in_fatigue = false;
        }
    }
}

void WeaponRuntime::start_attack(const AttackStageDef& stage) {
    is_attacking = true;
    recovery_timer = stage.recovery;
    // windup + hit_frame deferred to animation layer
}

void WeaponRuntime::advance(float combo_timeout) {
    const int max_stages = 3;
    combo_index = (combo_index + 1) % max_stages;
    combo_timer = combo_timeout;
}

void WeaponRuntime::reset_combo() {
    combo_index = 0;
    combo_timer = 0.0f;
}

bool WeaponRuntime::can_act() const {
    return recovery_timer <= 0.0f && !in_fatigue && !special.active;
}

// ── WeaponComponent ──

WeaponComponent::WeaponComponent() {
    _def = get_weapon_def("fist_basic");
    _runtime.current_weapon_id = "fist_basic";
}

const WeaponDef* WeaponComponent::_get_or_fist() const {
    if (_def) return _def;
    return get_weapon_def("fist_basic");
}

void WeaponComponent::equip(const std::string& weapon_id) {
    const WeaponDef* wdef = get_weapon_def(weapon_id);
    if (!wdef) return;
    _def = wdef;
    _runtime.current_weapon_id = weapon_id;
    _runtime.reset_combo();
}

void WeaponComponent::unequip() {
    _def = get_weapon_def("fist_basic");
    _runtime.current_weapon_id = "fist_basic";
    _runtime.reset_combo();
}

WeaponType WeaponComponent::weapon_type() const {
    return _get_or_fist()->type;
}

bool WeaponComponent::can_attack(double game_time) const {
    if (!_runtime.can_act()) return false;
    float cooldown = _runtime.combo_index == 0 ? 0.15f : 0.0f;
    return (game_time - _runtime.last_attack_time) >= cooldown;
}

void WeaponComponent::execute_attack(double game_time) {
    const WeaponDef* def = _get_or_fist();
    int stage_count = def->stage_count;
    const AttackStageDef& stage = def->stages[_runtime.combo_index];

    _runtime.start_attack(stage);
    _runtime.advance(def->combo_timeout);
    _runtime.last_attack_time = (float)game_time;

    // Fatigue for crossbow stage-3 (affix cd_reduce cuts fatigue)
    if (def->type == WeaponType::CROSSBOW && _runtime.combo_index == 0 && stage_count == 3) {
        _runtime.in_fatigue = true;
        float base_fatigue = 0.5f;
        if (def->affix.type == "cd_reduce")
            base_fatigue *= (1.0f - def->affix.value);
        _runtime.fatigue_timer = base_fatigue;
    }
}

void WeaponComponent::tick(float dt) {
    const WeaponDef* def = _get_or_fist();
    _runtime.tick(dt, def->combo_timeout);
}

const AttackStageDef& WeaponComponent::current_stage() const {
    const WeaponDef* def = _get_or_fist();
    int idx = std::min(_runtime.combo_index, def->stage_count - 1);
    return def->stages[idx];
}

// ── Backward-compat shims ──

float WeaponComponent::multiplier() const {
    return current_stage().damage_multiplier;
}

bool WeaponComponent::is_heavy() const {
    const WeaponDef* def = _get_or_fist();
    return _runtime.combo_index == 2 && def->stage_count >= 3;
}

void WeaponComponent::hit(double game_time) {
    const WeaponDef* def = _get_or_fist();
    // Advance combo on successful hit
    _runtime.advance(def->combo_timeout);
    _runtime.last_attack_time = (float)game_time;
}

// ── Hit shape queries ──

HitShape WeaponComponent::current_hit_shape() const {
    return current_stage().hit_shape;
}

float WeaponComponent::current_range() const {
    return current_stage().range;
}

float WeaponComponent::current_width() const {
    return current_stage().width;
}

float WeaponComponent::current_shake() const {
    return current_stage().camera_shake;
}

const char* WeaponComponent::current_sfx() const {
    return current_stage().sfx_name.c_str();
}
