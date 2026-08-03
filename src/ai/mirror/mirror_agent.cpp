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
    if (st.dist_tiles < 3.0f && _profile.attack_frequency > 0.6f)
        return PlayerActionType::ATTACK;
    if (st.player_hp_pct < _profile.hp_counter_threshold / 100.0f
        && _profile.heal_frequency > 0.01f)
        return PlayerActionType::HEAL;
    if (_profile.predict_skill_spam && _phase >= 2)
        return PlayerActionType::SKILL;
    return PlayerActionType::ATTACK;
}

// ── F15.4: Mirror reward ──
double MirrorAgent::mirror_reward(const PlayerHabitProfile& profile,
    int /*boss_action*/, double damage_dealt, double damage_taken,
    bool player_dodged, bool player_healed)
{
    double reward = damage_dealt * 0.5 - damage_taken * 0.3;

    // Counter bonuses
    if (profile.predict_panic_heal && player_healed)
        reward += 1.0;  // punished a panic-heal player
    if (profile.predict_low_dodge && player_dodged)
        reward += 0.5;  // predicted and hit a low-dodge player
    if (profile.predict_attack_heavy)
        reward += 0.3;  // aggressive profile = more opportunities to counter

    return reward;
}

int MirrorAgent::style_to_int(PlayerStyle s) {
    switch (s) {
    case PlayerStyle::AGGRESSIVE: return 1;
    case PlayerStyle::DEFENSIVE:  return 2;
    case PlayerStyle::SNIPER:     return 3;
    case PlayerStyle::MAGE:       return 4;
    default: return 0;
    }
}
