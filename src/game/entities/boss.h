#pragma once
#include <vector>
#include <memory>
#include <string>
#include "ai.h"

// 前向声明
class Monster;
class Player;
class GameMap;
struct Effect;

// ============================================================
// BossSkill — Boss 专属技能
// ============================================================
struct BossSkill {
    std::string name;
    float cooldown;
    float last_use_time = -999.0f;
    std::string fx_kind = "circle";
    float fx_radius = 60.0f;
    Color fx_color{200, 40, 40, 255};

    BossSkill(const std::string& n, float cd) : name(n), cooldown(cd) {}

    bool can_use(double t) const { return (t - last_use_time) >= cooldown; }
    void mark_used(double t) { last_use_time = t; }

    virtual std::string execute(Monster* boss, Player* player,
                                std::vector<Monster*>& monsters,
                                GameMap* map, double game_time) = 0;
};

class ConeAttack : public BossSkill {
public:
    ConeAttack();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
};

class CircleAOE : public BossSkill {
public:
    CircleAOE();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
};

class SummonMinions : public BossSkill {
public:
    SummonMinions();
    std::string execute(Monster* boss, Player* player,
                        std::vector<Monster*>& monsters,
                        GameMap* map, double game_time) override;
};

// ============================================================
// BossAI — 继承 MonsterAI，增加技能 + 狂暴
// ============================================================
class BossAI : public MonsterAI {
public:
    BossAI();
    void update(Monster* self, Player* player, GameMap* map,
                double dt, double game_time,
                std::vector<Monster*>* all_monsters = nullptr,
                std::vector<Effect>* effects = nullptr) override;

    std::vector<std::unique_ptr<BossSkill>> skills;
    bool is_enraged = false;

private:
    void _enter_enrage();
    float _hp_ratio(Monster* self) const;
};

// 工厂
Monster* spawn_boss(int tile_x, int tile_y, int floor);

struct BossInfo {
    std::string name, title, lore, skills_text;
    int max_hp, attack, pdef, mdef;
    Color color;
};
BossInfo get_boss_info(int floor);
