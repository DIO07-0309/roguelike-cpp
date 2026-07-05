#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <random>
#include "raylib.h"
#include "entity.h"
#include "combat_stats.h"

// 前向声明
class Player;
class GameMap;
class MonsterAI;

// ============================================================
// Monster — 怪物实体
// ============================================================
class Monster {
public:
    Entity entity;
    CombatStats combat;
    std::string name;
    Color color{200, 80, 80, 255};
    bool is_boss = false;
    AttackType attack_type = AttackType::PHYSICAL;
    float attack_cooldown = 1.5f;
    float last_attack_time = 0.0f;

    // AI 组件 (在 ai.h 中定义)
    class MonsterAI* ai = nullptr;

    Monster(float x, float y, const std::string& n, int hp, int atk,
            int pdef, int mdef, Color c, class MonsterAI* a = nullptr);
    ~Monster();

    void update_ai(Player* player, GameMap* map, double dt, double game_time,
                   std::vector<Monster*>* all_monsters = nullptr,
                   std::vector<struct Effect>* effects = nullptr);
    int attack_target(Player* target, double game_time);
    bool can_attack(double game_time) const;

    void draw(float cam_x, float cam_y);
};

// 前置声明特效结构
struct Effect;

// 工厂
Monster* spawn_monster(float px, float py, const std::string& type);
