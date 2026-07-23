#pragma once
// G8.1: BTAgent — Behavior Tree wrapper with same interface as DecisionAgent.
// Reads game state → writes to Blackboard → ticks BT → reads action from Blackboard.

#include "behavior_tree/behavior_tree.h"
#include <string>
#include <vector>
#include <memory>

class Player;
class Monster;
class GameMap;

class BTAgent {
public:
    BTAgent();
    ~BTAgent();

    // G8.1: Build the behavior tree (called once after construction)
    void build_tree();

    // Same interface as DecisionAgent::best_action()
    std::string best_action(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);

    // Event decision
    bool accept_event(float risk_pct, const std::string& effect_desc,
                      const Player* player) const;

    // Compat with GameScene._is_action_just_pressed
    bool is_action_just_pressed(const char* action_name,
        const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map,
        bool stairs_active, bool boss_intro_active);

    // G7.4 compat: start() for new run
    void start(const Player* player) { if (player) _resolve_profile(player); }

    // Tick frame counter (for rate-limiting)
    void tick() { _frame++; }

    // G8.1: access blackboard for testing
    const bt::Blackboard& board() const { return _board; }

private:
    bt::Blackboard _board;
    std::unique_ptr<bt::Node> _root;
    int _frame = 0;

    void _sync_state(const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active);
    void _resolve_profile(const Player* player);
    Monster* _find_nearest(const Player* player,
        const std::vector<Monster*>& monsters) const;
    float _hp_ratio(const Player* p) const;
    int _count_enemies_in_range(const Player* player,
        const std::vector<Monster*>& monsters, float range_px) const;
};
