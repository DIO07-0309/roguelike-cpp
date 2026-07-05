#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include "raylib.h"
#include "entity.h"

// 前向声明
class Player;
class Monster;
class GameMap;

// ============================================================
// Skill — 技能系统 (4主动 + 2被动 + 3级升级 + 冷却)
// ============================================================

class Skill {
public:
    std::string name;
    float base_cooldown;
    float cooldown;
    int level = 1;
    int max_level = 5;
    float last_use_time = -999.0f;

    float charm_cd_bonus = 0.0f;
    float charm_power_bonus = 0.0f;

    Skill(const std::string& n, float cd, int ml = 5)
        : name(n), base_cooldown(cd), cooldown(cd), max_level(ml) {}
    virtual ~Skill() = default;

    bool can_use(double game_time) const;
    float remaining_cooldown(double game_time) const;
    void mark_used(double game_time);
    bool can_upgrade() const { return level < max_level; }
    bool upgrade();

    float get_power_multiplier() const;
    virtual std::string get_level_text() const;

    void apply_charm(float cd_bonus, float power_bonus);
    void remove_charm();

protected:
    virtual void _on_level_up();
    void _recalc_cooldown();
};

// 主动技能
class ActiveSkill : public Skill {
public:
    ActiveSkill(const std::string& n, float cd, int ml = 5)
        : Skill(n, cd, ml) {}

    virtual std::string execute(Player* caster, std::vector<Monster*>& targets,
                                GameMap* map) = 0;
};

// 被动技能
class PassiveSkill : public Skill {
public:
    std::string modifier_key;
    int base_value;

    PassiveSkill(const std::string& n, const std::string& key, int val, int ml = 3)
        : Skill(n, 0.0f, ml), modifier_key(key), base_value(val) {}

    virtual int get_value() const;
    void apply(Player* player);
    void remove(Player* player);
};

// ---- 具体主动技能 ----
class SlashSkill : public ActiveSkill {
public:
    SlashSkill();
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map) override;
    std::string get_level_text() const override;
protected:
    void _on_level_up() override;
    int _cone_range = 1;
};

class FireballSkill : public ActiveSkill {
public:
    FireballSkill();
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map) override;
    std::string get_level_text() const override;
protected:
    void _on_level_up() override;
    int _target_count = 1;
    float _range = 6.0f;
};

class SelfHealSkill : public ActiveSkill {
public:
    SelfHealSkill();
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map) override;
    std::string get_level_text() const override;
    void tick_regen(Player* caster, float delta);
protected:
    void _on_level_up() override;
    float _regen_left = 0.0f;
};

class TheWorldSkill : public ActiveSkill {
public:
    TheWorldSkill();
    std::string execute(Player* caster, std::vector<Monster*>& targets,
                        GameMap* map) override;
    std::string get_level_text() const override;
    float get_stop_duration() const;
protected:
    void _on_level_up() override;
    float _stop_duration[4] = {0.0f, 5.0f, 7.0f, 10.0f};
};

// ---- 具体被动技能 ----
class IronSkinSkill : public PassiveSkill {
public:
    IronSkinSkill() : PassiveSkill("铁壁", "def_flat", 3, 3) {}
    int get_value() const override {
        int v[] = {0, 3, 7, 12};
        return v[std::min(level, 3)];
    }
    std::string get_level_text() const override {
        return "Lv" + std::to_string(level) + " DEF+" + std::to_string(get_value());
    }
};

class BerserkSkill : public PassiveSkill {
public:
    BerserkSkill() : PassiveSkill("狂暴", "atk_flat", 3, 3) {}
    int get_value() const override {
        int v[] = {0, 3, 7, 12};
        return v[std::min(level, 3)];
    }
    std::string get_level_text() const override {
        return "Lv" + std::to_string(level) + " ATK+" + std::to_string(get_value());
    }
};

// ---- 技能管理器 ----
class SkillManager {
public:
    std::vector<std::unique_ptr<ActiveSkill>> active_skills;
    std::vector<std::unique_ptr<PassiveSkill>> passives;
    int max_active = 4;

    bool can_learn() const { return (int)active_skills.size() < max_active; }
    bool learn(std::unique_ptr<Skill> skill);
    std::string use_active(int index, Player* caster,
                           std::vector<Monster*>& targets,
                           GameMap* map, double game_time);
    void apply_all_passives(Player* player);
};

// 工厂
std::unique_ptr<ActiveSkill> random_active_skill(const std::vector<std::string>& exclude = {});
std::unique_ptr<Skill> random_skill(const std::vector<std::string>& exclude = {});
std::vector<std::string> get_learned_names(const SkillManager& mgr);
