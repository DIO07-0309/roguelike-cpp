#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "raylib.h"
#include "entity.h"
#include "combat_stats.h"
#include "inventory.h"
#include "skill.h"
#include "input_map.h"

// ============================================================
// Player — 玩家实体
// ============================================================
class Player {
public:
    Entity entity;
    float speed = 200.0f;
    Direction direction = Direction::DOWN;
    bool is_moving = false;
    CombatStats combat;
    std::vector<BuffInstance> active_buffs;   // 当前施加的 buff
    Inventory inventory;
    SkillManager skills;
    AttackType attack_type = AttackType::PHYSICAL;

    int level = 1;
    int xp = 0;
    int xp_to_next = 80;

    static constexpr float ATTACK_COOLDOWN = 0.5f;
    float _last_attack_time = -999.0f;

    // 构造
    Player(float x, float y, float spd, int hp, int atk, int pdef, int mdef);

    void reset_attack_timers();
    bool can_attack(double game_time) const;
    int attack_target(Player* target, double game_time) { return 0; } // 玩家打怪物用 combat_system

    // 移动
    Vector2 handle_input(const InputMap& input);

    // 等级
    static int calc_xp_for_level(int lvl) { return lvl * lvl * 20; }
    void give_xp(int amount);
    void auto_level_to(int target);

    // 渲染
    void render(Camera2D& cam) {}
    void draw_no_cam(float cam_x, float cam_y);
};
