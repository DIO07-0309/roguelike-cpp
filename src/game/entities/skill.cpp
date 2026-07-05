#include "skill.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include <cmath>
#include <algorithm>

// ---- Skill 基类 ----
bool Skill::can_use(double game_time) const {
    return (game_time - last_use_time) >= cooldown;
}
float Skill::remaining_cooldown(double game_time) const {
    return std::max(0.0f, (float)(cooldown - (game_time - last_use_time)));
}
void Skill::mark_used(double game_time) { last_use_time = (float)game_time; }

bool Skill::upgrade() {
    if (!can_upgrade()) return false;
    level++;
    _on_level_up();
    return true;
}

float Skill::get_power_multiplier() const {
    float m[] = {1.0f, 1.0f, 1.4f, 2.0f};
    return m[std::min(level, 3)] * (1.0f + charm_power_bonus);
}

std::string Skill::get_level_text() const {
    return "Lv" + std::to_string(level) + " x" + std::to_string(get_power_multiplier());
}

void Skill::_on_level_up() {
    base_cooldown = std::max(0.15f, base_cooldown * 0.8f);
    _recalc_cooldown();
}

void Skill::_recalc_cooldown() {
    cooldown = std::max(0.1f, base_cooldown * (1.0f - charm_cd_bonus));
}

void Skill::apply_charm(float cd, float pw) {
    charm_cd_bonus = cd; charm_power_bonus = pw;
    _recalc_cooldown();
}

void Skill::remove_charm() {
    charm_cd_bonus = 0; charm_power_bonus = 0;
    _recalc_cooldown();
}

// ---- PassiveSkill ----
int PassiveSkill::get_value() const {
    return (int)(base_value * get_power_multiplier());
}

void PassiveSkill::apply(Player* player) {
    int v = get_value();
    auto& mod = player->combat.modifiers;
    mod[modifier_key] = (mod.count(modifier_key) ? mod[modifier_key] : 0) + v;
}

void PassiveSkill::remove(Player* player) {
    auto& mod = player->combat.modifiers;
    mod[modifier_key] = mod[modifier_key] - get_value();
}

// ---- SlashSkill ----
SlashSkill::SlashSkill() : ActiveSkill("斩击", 2.0f, 3) { _cone_range = 1; }

void SlashSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 1.6f; _cone_range = 2; }
    else if (level == 3) { base_cooldown = 1.2f; _cone_range = 2; }
    _recalc_cooldown();
}

std::string SlashSkill::execute(Player* caster, std::vector<Monster*>& targets, GameMap*) {
    auto hit = get_targets_in_cone(caster, targets, _cone_range);
    if (hit.empty()) return "斩击挥空了（无目标）";
    int total = 0;
    float base = caster->combat.get_effective_attack() * 1.5f;
    float power = get_power_multiplier();
    int hits_per = (level >= 3) ? 2 : 1;
    for (auto* t : hit) {
        for (int i = 0; i < hits_per; i++) {
            int dmg = calculate_damage((int)(base * power),
                t->combat.get_effective_defense(AttackType::PHYSICAL));
            t->combat.take_damage(dmg);
            total += dmg;
        }
    }
    std::string d = "斩击Lv" + std::to_string(level) + " 命中 " + std::to_string(hit.size()) + " 目标";
    if (hits_per > 1) d += "（二连击）";
    return d + "，造成 " + std::to_string(total) + " 伤害";
}

std::string SlashSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " 范围" + std::to_string(_cone_range) + "格 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- FireballSkill ----
FireballSkill::FireballSkill() : ActiveSkill("神罚", 5.0f, 3) {}

void FireballSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 4.0f; _target_count = 2; _range = 7.0f; }
    else if (level == 3) { base_cooldown = 3.0f; _target_count = 3; _range = 8.0f; }
    _recalc_cooldown();
}

std::string FireballSkill::execute(Player* caster, std::vector<Monster*>& targets, GameMap*) {
    std::vector<Monster*> alive;
    for (auto* t : targets) if (t && t->combat.is_alive) alive.push_back(t);

    float pcx = caster->entity.rect.x + caster->entity.rect.width/2;
    float pcy = caster->entity.rect.y + caster->entity.rect.height/2;
    std::sort(alive.begin(), alive.end(), [&](Monster* a, Monster* b) {
        float da = hypotf(a->entity.rect.x + a->entity.rect.width/2 - pcx,
                          a->entity.rect.y + a->entity.rect.height/2 - pcy);
        float db = hypotf(b->entity.rect.x + b->entity.rect.width/2 - pcx,
                          b->entity.rect.y + b->entity.rect.height/2 - pcy);
        return da < db;
    });

    std::vector<Monster*> chosen;
    float range_px = _range * 32.0f;
    for (auto* t : alive) {
        if ((int)chosen.size() >= _target_count) break;
        float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - pcx,
                         t->entity.rect.y + t->entity.rect.height/2 - pcy);
        if (d <= range_px) chosen.push_back(t);
    }
    if (chosen.empty()) return "神罚没有击中任何目标";

    int total = 0;
    float base = caster->combat.get_effective_attack() * 2.5f;
    float power = get_power_multiplier();
    for (auto* t : chosen) {
        int dmg = calculate_damage((int)(base * power),
            t->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
        t->combat.take_damage(dmg);
        total += dmg;
    }
    return "神罚Lv" + std::to_string(level) + " 命中 " + std::to_string(chosen.size())
           + " 目标，造成 " + std::to_string(total) + " 伤害";
}

std::string FireballSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " " + std::to_string(_target_count) + "目标 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- SelfHealSkill ----
SelfHealSkill::SelfHealSkill() : ActiveSkill("自愈", 8.0f, 3) {}

void SelfHealSkill::_on_level_up() {
    if (level == 2) base_cooldown = 6.5f;
    else if (level == 3) base_cooldown = 5.0f;
    _recalc_cooldown();
}

std::string SelfHealSkill::execute(Player* caster, std::vector<Monster*>&, GameMap*) {
    float pcts[] = {0, 0.20f, 0.35f, 0.35f};
    float pct = pcts[std::min(level, 3)];
    float power = get_power_multiplier();
    int instant = (int)(caster->combat.max_hp * pct * power);
    int recovered = caster->combat.heal(instant);
    std::string d = "自愈Lv" + std::to_string(level) + " 瞬回 " + std::to_string(recovered) + " HP";
    if (level >= 3) { _regen_left = 4.0f; d += "（+4秒持续再生）"; }
    return d;
}

void SelfHealSkill::tick_regen(Player* caster, float dt) {
    if (_regen_left <= 0) return;
    _regen_left -= dt;
    int heal_per = (int)(caster->combat.max_hp * 0.03f * dt);
    if (heal_per > 0) caster->combat.heal(heal_per);
}

std::string SelfHealSkill::get_level_text() const {
    std::string s = "Lv" + std::to_string(level) + " " + std::to_string(cooldown).substr(0,3) + "s";
    if (level >= 3) s += "+再生";
    return s;
}

// ---- TheWorldSkill ----
TheWorldSkill::TheWorldSkill() : ActiveSkill("The World", 20.0f, 3) {}
void TheWorldSkill::_on_level_up() {
    if (level == 2) base_cooldown = 16.0f;
    else if (level == 3) base_cooldown = 12.0f;
    _recalc_cooldown();
}
float TheWorldSkill::get_stop_duration() const {
    return _stop_duration[std::min(level, 3)] * (1.0f + charm_power_bonus);
}
std::string TheWorldSkill::execute(Player*, std::vector<Monster*>&, GameMap*) {
    return "__TIME_STOP__";
}
std::string TheWorldSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " 停" + std::to_string((int)get_stop_duration()) + "s " + std::to_string(cooldown).substr(0,4) + "s";
}

// ---- SkillManager ----
bool SkillManager::learn(std::unique_ptr<Skill> skill) {
    if (auto* p = dynamic_cast<PassiveSkill*>(skill.get())) {
        passives.push_back(std::unique_ptr<PassiveSkill>(p));
        skill.release();
        return true;
    }
    if (auto* a = dynamic_cast<ActiveSkill*>(skill.get())) {
        if (!can_learn()) return false;
        active_skills.push_back(std::unique_ptr<ActiveSkill>(a));
        skill.release();
        return true;
    }
    return false;
}

std::string SkillManager::use_active(int idx, Player* caster,
                                      std::vector<Monster*>& targets,
                                      GameMap* map, double gt) {
    if (idx < 0 || idx >= (int)active_skills.size()) return "";
    auto& sk = active_skills[idx];
    if (!sk->can_use(gt)) {
        float r = sk->remaining_cooldown(gt);
        return sk->name + " 冷却中 (" + std::to_string(r).substr(0,3) + "s)";
    }
    std::string result = sk->execute(caster, targets, map);
    sk->mark_used(gt);
    return result;
}

void SkillManager::apply_all_passives(Player* player) {
    for (auto& p : passives) p->apply(player);
}

// ---- 工厂 ----
std::unique_ptr<ActiveSkill> random_active_skill(const std::vector<std::string>& exclude) {
    std::vector<std::unique_ptr<ActiveSkill>> pool;
    // 用类型标记来区分，简化起见按顺序
    bool has[] = {false, false, false, false}; // Slash, Fireball, SelfHeal, TheWorld
    for (auto& s : exclude) {
        if (s == "SlashSkill") has[0] = true;
        else if (s == "FireballSkill") has[1] = true;
        else if (s == "SelfHealSkill") has[2] = true;
        else if (s == "TheWorldSkill") has[3] = true;
    }
    if (!has[0]) pool.push_back(std::make_unique<SlashSkill>());
    if (!has[1]) pool.push_back(std::make_unique<FireballSkill>());
    if (!has[2]) pool.push_back(std::make_unique<SelfHealSkill>());
    if (!has[3]) pool.push_back(std::make_unique<TheWorldSkill>());
    if (pool.empty()) pool.push_back(std::make_unique<SlashSkill>());

    int idx = (int)(rng() % pool.size());
    auto out = std::move(pool[idx]);
    return out;
}

std::unique_ptr<Skill> random_skill(const std::vector<std::string>& exclude) {
    if ((rng() % 100) < 70)
        return random_active_skill(exclude);
    if ((rng() % 2) == 0)
        return std::make_unique<IronSkinSkill>();
    return std::make_unique<BerserkSkill>();
}

std::vector<std::string> get_learned_names(const SkillManager& mgr) {
    std::vector<std::string> names;
    for (auto& s : mgr.active_skills) {
        if (dynamic_cast<SlashSkill*>(s.get())) names.push_back("SlashSkill");
        else if (dynamic_cast<FireballSkill*>(s.get())) names.push_back("FireballSkill");
        else if (dynamic_cast<SelfHealSkill*>(s.get())) names.push_back("SelfHealSkill");
        else if (dynamic_cast<TheWorldSkill*>(s.get())) names.push_back("TheWorldSkill");
    }
    return names;
}

// ---- 锥形目标判定 ----
std::vector<Monster*> get_targets_in_cone(Player* caster,
    const std::vector<Monster*>& targets, int cone_range) {
    std::vector<Monster*> result;
    float cx = caster->entity.rect.x + caster->entity.rect.width/2;
    float cy = caster->entity.rect.y + caster->entity.rect.height/2;
    float cr = cone_range * 48.0f;  // 1.5 tiles per range unit

    for (auto* t : targets) {
        if (!t || !t->combat.is_alive) continue;
        float tx = t->entity.rect.x + t->entity.rect.width/2;
        float ty = t->entity.rect.y + t->entity.rect.height/2;
        float dx = tx - cx, dy = ty - cy;
        float dist = sqrtf(dx*dx + dy*dy);
        if (dist > cr) continue;

        // 检查是否在朝向的锥形区域
        bool in_cone = false;
        switch (caster->direction) {
            case Direction::DOWN:  in_cone = (dy > 0); break;
            case Direction::UP:    in_cone = (dy < 0); break;
            case Direction::RIGHT: in_cone = (dx > 0); break;
            case Direction::LEFT:  in_cone = (dx < 0); break;
        }
        if (in_cone) result.push_back(t);
    }
    return result;
}
