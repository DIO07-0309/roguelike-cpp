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
    // Returns recommended pressure distance (BossAI uses this for chase/retreat)
    float recommend_distance() const;

    // Returns true if Boss should prioritize interrupting player skill
    bool should_interrupt_skill() const;

    // Returns true if Boss should be aggressive right now (force close range)
    bool should_pressure_close(const MirrorBattleState& st) const;

    // Predict player's likely next action type after seeing current state
    PlayerActionType predict_next_action(const MirrorBattleState& st) const;

    // ── Debug ──
    const char* phase_name() const;
    const char* strategy_summary() const { return _profile.counter_strategy_text(); }

private:
    PlayerHabitProfile _profile;
    int  _phase = 1;          // 1=Observe, 2=Mirror, 3=Evolve
    float _phase_timer = 0.0f;
    float _phase_duration = 30.0f;
    float _preferred_distance = 250.0f; // pixels (derived from profile)
};
