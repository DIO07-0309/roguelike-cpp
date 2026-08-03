#pragma once
#include "ai/player_behavior/player_habit_profile.h"
#include "ai/player_behavior/player_action.h"

class Monster;
class Player;

// ============================================================
// F15.3: MirrorAgent — analysis layer, NOT a control layer
// Reads player habits → adjusts BossAI strategy parameters.
// Does NOT call attack/move — BossAI still makes final decisions.
// ============================================================

// Simple battle state snapshot (avoid depending on full BattleState)
struct MirrorBattleState {
    float boss_hp_pct = 1.0f;
    float player_hp_pct = 1.0f;
    float dist_tiles = 5.0f;
    bool  player_attacking = false;
    bool  player_using_skill = false;
    bool  boss_can_attack = false;
    bool  boss_in_domain = false;       // F10 domain phase
};

class MirrorAgent {
public:
    MirrorAgent();

    // ── Initialize from player behavior data (called on F15 enter) ──
    void init(const PlayerHabitProfile& profile);

    // ── Phase 1-2-3 behavior selection ──
    int  current_phase() const { return _phase; }   // 1=observe, 2=mirror, 3=evolve
    void set_phase(int p) { _phase = p; }
    void tick_phase_timer(float dt);

    // ── During combat: recommend BossAI adjustments ──
    float recommend_distance() const;
    bool should_interrupt_skill() const;
    bool should_pressure_close(const MirrorBattleState& st) const;
    PlayerActionType predict_next_action(const MirrorBattleState& st) const;

    // ── F15.4: Mirror reward for RL self-play ──
    // Returns a bonus reward when the Boss successfully counters the player's
    // predicted action (interrupting a skill, punishing a heal, baiting an attack).
    // Caller adds this to the base damage-healing reward.
    static double mirror_reward(const PlayerHabitProfile& profile,
        int boss_action, double damage_dealt, double damage_taken,
        bool player_dodged, bool player_healed);

    // ── F15.4: Convert PlayerStyle to an int for SimulationState ──
    static int style_to_int(PlayerStyle s);

    // ── Debug ──
    const char* phase_name() const;
    const char* strategy_summary() const { return _profile.counter_strategy_text(); }

private:
    PlayerHabitProfile _profile;
    int  _phase = 1;
    float _phase_timer = 0.0f;
    float _phase_duration = 30.0f;
    float _preferred_distance = 250.0f;
};
