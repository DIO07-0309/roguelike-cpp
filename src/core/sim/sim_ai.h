#pragma once
#include <cstdint>
#include <vector>
#include <string>

class Player;
class Monster;
class GameMap;
enum class BuildType;

// ============================================================
// G7.4: Decision Agent — build-aware AI with action evaluation
// Replaces G5.6 SimAI with behavioral profiles per BuildType.
// ============================================================

struct ActionScore {
    float value = 0;
    const char* action = "";
};

class DecisionAgent {
public:
    DecisionAgent();

    void start(const Player* player);
    void tick();

    // ── Main entry: returns the best action for this frame ──
    std::string best_action(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);

    // ── Event decision (G7.4) ──
    bool accept_event(float risk_pct, const std::string& effect_desc,
                      const Player* player) const;

    // ── Fake input gate (compat with existing _is_action_just_pressed) ──
    bool is_action_just_pressed(const char* action_name,
        const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map,
        bool stairs_active,
        bool boss_intro_active);

private:
    int  _frame = 0;
    float _dir_timer = 0;
    int  _current_dir = -1;

    // G7.4: Build-aware profile
    BuildType _build_type = (BuildType)0;
    float _prefer_range = 0;      // 0=melee aggro, 1=kite & keep distance
    float _prefer_aoe = 0;        // 0=single target, 1=fight groups
    float _prefer_skill = 0;      // 0=basic attacks, 1=skills first
    float _aggro_bias = 0.5f;     // how aggressively to approach enemies
    float _prefer_heal = 0;       // heal threshold (HP% below which heal used)
    int _skill_priority[4] = {0,1,2,3}; // skill index priority order

    // G7.4: Action evaluation
    float _evaluate_attack(const Player* p, const std::vector<Monster*>& monsters) const;
    float _evaluate_skill(int slot, const Player* p,
                          const std::vector<Monster*>& monsters) const;
    float _evaluate_move(int dir, const Player* p,
                         const std::vector<Monster*>& monsters,
                         const GameMap* map) const;
    float _evaluate_pickup(const Player* p, const GameMap* map) const;

    // Helpers
    Monster* _find_nearest(const Player* player,
                           const std::vector<Monster*>& monsters) const;
    int _count_in_range(const Player* player,
                        const std::vector<Monster*>& monsters, float range_px) const;
    float _hp_ratio(const Player* p) const;
    void _pick_direction(const Player* player,
                         const std::vector<Monster*>& monsters);
    void _resolve_profile(const Player* player);
};

// ── Backward compat alias ──
using SimAI = DecisionAgent;
