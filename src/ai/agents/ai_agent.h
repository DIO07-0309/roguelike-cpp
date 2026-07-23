#pragma once
// G8.1: Common AI agent interface — enables A/B testing (--sim-ai decision|bt|mcts)

#include <string>
#include <vector>
#include <memory>

class Player;
class Monster;
class GameMap;

struct AIAgent {
    virtual ~AIAgent() = default;
    virtual bool is_action_just_pressed(const char* action_name,
        const Player* player, const std::vector<Monster*>& monsters,
        const GameMap* map, bool stairs_active, bool boss_intro_active) = 0;
    virtual void start(const Player* player) = 0;
    virtual std::string name() const = 0;
};
