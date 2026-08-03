#include "ai/mirror/mirror_agent.h"
#include <algorithm>
#include <cstdio>

MirrorAgent::MirrorAgent() = default;

void MirrorAgent::init(const PlayerHabitProfile& profile) {
    _profile = profile;
    _phase = 1;
    _phase_timer = 0.0f;

    // Derive preferred distance from profile
    if (_profile.style == PlayerStyle::AGGRESSIVE)
        _preferred_distance = 180.0f;   // stay mid-close, bait attacks
    else if (_profile.style == PlayerStyle::SNIPER)
        _preferred_distance = 320.0f;   // keep them cornered, close the gap
    else if (_profile.style == PlayerStyle::MAGE)
        _preferred_distance = 250.0f;   // pressure at mid-range, interrupt casts
    else if (_profile.style == PlayerStyle::DEFENSIVE)
        _preferred_distance = 150.0f;   // force them into a corner, no escape
    else
        _preferred_distance = 230.0f;   // balanced
}

void MirrorAgent::tick_phase_timer(float dt) {
    _phase_timer += dt;
    // Phase transitions:
    // Phase 1 → 2 at 30s (observe current patterns)
    // Phase 2 → 3 at 60s (start using history data for counters)
    if (_phase == 1 && _phase_timer >= _phase_duration) {
        _phase = 2;
        _phase_timer = 0.0f;
    } else if (_phase == 2 && _phase_timer >= _phase_duration) {
        _phase = 3;
        _phase_timer = 0.0f;
    }
}

const char* MirrorAgent::phase_name() const {
    if (_phase == 1) return "Observe";
    if (_phase == 2) return "Mirror";
    return "Evolve";
}

float MirrorAgent::recommend_distance() const {
    // Phase 1: match player's preferred distance exactly (mirror)
    // Phase 2: adjust slightly to counter
    // Phase 3: actively close/pull based on weakness
    float base = _preferred_distance;
    if (_phase >= 2 && _profile.predict_low_dodge)
        base *= 0.7f;  // pressure closer if they dodge poorly
    if (_phase >= 3 && _profile.predict_attack_heavy)
        base *= 1.25f; // stay safer distance vs hyper-aggressive
    return base;
}

bool MirrorAgent::should_interrupt_skill() const {
    if (_phase < 2) return false;
    // If player spams a single skill >60%, prepare to interrupt it
    return _profile.predict_skill_spam;
}

bool MirrorAgent::should_pressure_close(const MirrorBattleState& st) const {
    if (_phase < 2) return false;
    // Pressure close: player HP is low and they tend to heal at this threshold
    if (st.player_hp_pct * 100.0f < _profile.hp_counter_threshold
        && _profile.heal_frequency > 0.01f)
        return true;
    // Phase 3: always pressure if player has low dodge
    if (_phase >= 3 && _profile.predict_low_dodge)
        return true;
    return false;
}

PlayerActionType MirrorAgent::predict_next_action(
    const MirrorBattleState& st) const {
    // Simple frequency-based prediction
    // Higher phase = more confident (use actual frequencies)

    // If the player is at range, they'll likely attack
    if (st.dist_tiles < 3.0f && _profile.attack_frequency > 0.6f)
        return PlayerActionType::ATTACK;

    // If their HP is low and they heal often
    if (st.player_hp_pct < _profile.hp_counter_threshold / 100.0f
        && _profile.heal_frequency > 0.01f)
        return PlayerActionType::HEAL;

    // If they have a favorite skill spammed >60%
    if (_profile.predict_skill_spam && _phase >= 2)
        return PlayerActionType::SKILL;

    // Default: they'll attack
    return PlayerActionType::ATTACK;
}
