#pragma once
// M1: BehaviorCloneTable — Player Clone Agent first learning layer.
// Learns a state -> player-intention distribution from the F1-F14
// PlayerAction stream (behavioral cloning, no neural network).
// Degradation chain: exact state -> fuzzy state -> PlayerHabitProfile -> default.
#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include "ai/player_behavior/player_action.h"
#include "ai/player_behavior/player_habit_profile.h"

// ── Player battle intent (what the player is about to do) ──
enum class PlayerIntention : int {
    ATTACK = 0,   // weapon combo hit
    SKILL,        // active skill
    DODGE,        // evasive dodge
    HEAL,         // self heal / potion
    ADVANCE,      // move toward the enemy
    RETREAT,      // move away from the enemy
    IDLE,         // no combat intent
    COUNT
};

const char* intention_name(PlayerIntention i);

// Map a recorded raw action onto a learned intent.
// Returns IDLE for non-decision actions (they are filtered out when learning).
PlayerIntention intention_from_action(PlayerActionType type);

// ============================================================
// CloneContext — interpretable observation key
// key: "d<dist>:h<hp>:s<skills>" e.g. "d1:h0:s3"
// dist bucket: 0 贴身(<2格) 1 近(2-4) 2 中(4-8) 3 远(8-14) 4 极远(>=14)
// hp   bucket: 0 危急(<25%) 1 低(25-50%) 2 中(50-80%) 3 高(>=80%)
// skill bucket: 0..3 (ready skill count, >=3 truncated)
// ============================================================
struct CloneContext {
    int dist = 2;
    int hp = 2;
    int skill = 0;

    static CloneContext from_state(float dist_tiles, float hp_pct, int skills_ready);
    std::string key() const;             // exact key "d2:h3:s1"
    std::string fuzzy_key() const;       // merged skill "d2:h3"
};

// ============================================================
// Table + prediction
// ============================================================
struct ClonePrediction {
    PlayerIntention best = PlayerIntention::IDLE;
    float confidence = 0.0f;   // best-intent count / total in that entry
    int level = 0;             // 0=exact, 1=fuzzy, 2=profile, 3=default
};

class BehaviorCloneTable {
public:
    using Counts = std::array<int, (int)PlayerIntention::COUNT>;

    void clear();
    void set_profile(const PlayerHabitProfile& profile) { profile_ = profile; _has_profile = true; }

    // Build state->intent distribution from a replayable action stream
    void build(const std::vector<PlayerAction>& stream);

    // Train one decision sample directly (used by tests / online updates)
    void record_decision(const CloneContext& ctx, PlayerIntention intent);

    // Full degradation-chain prediction
    ClonePrediction predict(float dist_tiles, float hp_pct, int skills_ready) const;

    size_t entries() const { return _table.size(); }
    const PlayerHabitProfile* profile() const { return _has_profile ? &profile_ : nullptr; }

    // M4: attack/retreat classification hook is pure data — kept for intent enum completeness
    static PlayerIntention fallback_intent(const PlayerHabitProfile& p,
        float dist_tiles, float hp_pct);   // profile-level default policy

private:
    std::unordered_map<std::string, Counts> _table;
    PlayerHabitProfile profile_;
    bool _has_profile = false;

    static void _bump(Counts& c, PlayerIntention i) {
        c[(int)i]++;
    }
    static bool _pick_best(const Counts& c, ClonePrediction& out);
    ClonePrediction _predict_exact(const CloneContext& ctx) const;
    ClonePrediction _predict_fuzzy(const CloneContext& ctx) const;
};