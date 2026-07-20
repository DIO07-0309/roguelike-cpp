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
#include "attack_evolution_state.h"   // G1: 普攻进化

// ============================================================
// D2: ComboState — 四段连击状态 (挂在 Player 上)
// 后续 D3~D5 的 Skill/Boss/Relic 全部可以查询此状态
// ============================================================
struct ComboState {
    int count = 0;           // 当前连击段数: 0=站立, 1-3=普通连击, 4=重击
    float timer = 0.0f;      // 连击窗口剩余时间
    float last_hit_time = 0;

    static constexpr float WINDOW = 0.85f;
    static constexpr float COOLDOWN = 0.35f;

    float multiplier() const;   // 当前段伤害倍率: 1.0/1.15/1.4/2.2
    bool  is_heavy() const;     // 是否为重击段 (count==4)
    void  hit(double game_time); // 命中: 推进连击段数 + 重置窗口
    void  tick(float dt);        // 逐帧衰减窗口
};

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
    std::vector<RelicInstance> relics;        // B11: 局内圣物 (跨楼层持续)
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
    int attack_target(Player* target, double game_time);

    // 移动
    Vector2 handle_input(const InputMap& input);

    // 等级
    static int calc_xp_for_level(int lvl);
    void give_xp(int amount);
    void auto_level_to(int target);

    // 渲染
    void render(Camera2D& cam);
    void draw_no_cam(float cam_x, float cam_y);

    // D2: 连击状态
    ComboState combo;

    // G1: 普攻进化状态
    AttackEvolutionState attack_evo;

    // D2 Step2: 消耗重击标记 (技能/Relic/Boss 统一调用此接口)
    bool consume_heavy_combo();
};
