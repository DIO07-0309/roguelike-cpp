#pragma once
#include <cstdint>
#include <vector>
#include <random>
#include "raylib.h"

// 前向声明
class Monster;
class Player;
class GameMap;
struct Effect;

// ============================================================
// MonsterAI — 怪物行为状态机 (IDLE → CHASE → ATTACK)
// ============================================================
enum class AIState { IDLE, CHASE, ATTACK };

class MonsterAI {
public:
    AIState state = AIState::IDLE;
    float sight_range = 6.0f;
    float move_speed = 80.0f;
    float patrol_interval = 2.0f;
    float attack_range = 1.5f;

    MonsterAI(float sight = 6.0f, float speed = 80.0f, float patrol = 2.0f,
              float attack = 1.5f);
    virtual ~MonsterAI() = default;

    virtual void update(Monster* self, Player* player, GameMap* map,
                        double dt, double game_time,
                        std::vector<Monster*>* all_monsters = nullptr,
                        std::vector<Effect>* effects = nullptr);

protected:
    float _patrol_timer = 0.0f;
    Vector2 _patrol_dir{0, 0};

    void _decide_state(Monster* self, Player* player);
    void _execute_idle(Monster* self, GameMap* map, double dt);
    void _execute_chase(Monster* self, Player* player, GameMap* map, double dt);
    void _execute_attack(Monster* self, Player* player, double game_time,
                         std::vector<Effect>* effects);
    void _apply_movement(Monster* self, GameMap* map, float mx, float my, double dt);
    float _dist_to(Monster* self, Player* player) const;
    void _pick_new_dir();
};
