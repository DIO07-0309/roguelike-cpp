#pragma once
#include <cstdint>
#include <vector>

class Player;
class Monster;
class GameMap;
struct InputMap;

// ============================================================
// G5.6: SimAI — simple AI that drives the player in sim mode
// Hooks into _is_action_just_pressed gate (same as ReplayPlayer)
// ============================================================

class SimAI {
public:
    void start();
    void tick();

    // ── fake input gate ──
    bool is_action_just_pressed(const char* action_name,
        const Player* player,
        const std::vector<Monster*>& monsters,
        const GameMap* map,
        bool stairs_active,
        bool boss_intro_active);

private:
    int  _frame = 0;
    float _dir_timer = 0;      // direction change cooldown
    int  _current_dir = -1;    // 0=up,1=down,2=left,3=right, -1=none

    void _pick_direction(const Player* player, const std::vector<Monster*>& monsters);
    Monster* _find_nearest(const Player* player, const std::vector<Monster*>& monsters) const;
};
