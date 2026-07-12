#include "skill.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"
#include <cmath>
#include <algorithm>

// ---- 从 skill.h 移出的实现 ----

// ============================================================
// D3 Step2: Evolution 实现
// ============================================================
void Skill::add_evolution(int lv, const char* name, const char* desc,
                           std::initializer_list<BuildTag> req) {
    SkillEvolution ev;
    ev.level = lv; ev.name = name; ev.desc = desc;
    for (auto t : req) ev.required_tags.push_back(t);
    evolutions.push_back(ev);
}

bool Skill::can_evolve() const {
    return evolution_level < (int)evolutions.size();
}

std::string Skill::evolve() {
    if (!can_evolve()) return "";
    evolution_level++;
    const SkillEvolution* ev = current_evolution();
    return ev ? ev->name : "";
}

const SkillEvolution* Skill::current_evolution() const {
    if (evolution_level <= 0 || evolution_level > (int)evolutions.size()) return nullptr;
    return &evolutions[evolution_level - 1];
}

std::string Skill::get_evolution_text() const {
    if (evolution_level <= 0) return "";
    const SkillEvolution* ev = current_evolution();
    if (!ev) return "Evo" + std::to_string(evolution_level);
    return ev->name;
}

// Skill 基类
Skill::Skill(const std::string& n, float cd, int ml)
    : name(n), base_cooldown(cd), cooldown(cd), max_level(ml) {}
bool Skill::can_upgrade() const { return level < max_level; }

// SkillManager
bool SkillManager::can_learn() const { return (int)active_skills.size() < max_active; }

// IronSkinSkill
int IronSkinSkill::get_value() const {
    int v[] = {0, 3, 7, 12};
    return v[std::min(level, 3)];
}
std::string IronSkinSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " DEF+" + std::to_string(get_value());
}

// BerserkSkill
int BerserkSkill::get_value() const {
    int v[] = {0, 3, 7, 12};
    return v[std::min(level, 3)];
}
std::string BerserkSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " ATK+" + std::to_string(get_value());
}

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
SlashSkill::SlashSkill() : ActiveSkill("斩击", 2.0f, 3) {
    _cone_range = 1;
    triggers = {{"poison", 1, 0.30f, BuffTarget::ENEMY}};
    tags = {BuildTag::MELEE, BuildTag::COMBO};
    // D3 Step2: Evolution path
    add_evolution(1, "广域斩", "攻击范围+30%", {BuildTag::MELEE});
    add_evolution(2, "流血斩", "100%附加1层Bleed", {BuildTag::MELEE, BuildTag::BLEED});
    add_evolution(3, "冲击斩", "Heavy: 冲击波向前延伸", {BuildTag::MELEE, BuildTag::COMBO, BuildTag::HEAVY});
}

void SlashSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 1.6f; _cone_range = 2; }
    else if (level == 3) { base_cooldown = 1.2f; _cone_range = 2; }
    _recalc_cooldown();
}

std::string SlashSkill::execute(Player* caster, std::vector<Monster*>& targets,
                                 GameMap*, bool is_heavy) {
    int cr = _cone_range;
    float dmg_bonus = 1.0f;
    // D3 Step3 E1: 范围+30%
    if (evolution_level >= 1) cr = (int)(cr * 1.3f);
    // D2 Step2: Heavy → 范围+40%, 伤害+30%
    if (is_heavy) { cr = (int)(cr * 1.4f); if (cr < 2) cr = 2; dmg_bonus = 1.3f; }
    auto hit = get_targets_in_cone(caster, targets, cr);
    if (hit.empty()) return is_heavy ? "重·斩击挥空了" : "斩击挥空了（无目标）";

    std::string extra;
    for (auto* t : hit) {
        for (auto& tr : triggers) {
            if (tr.target == BuffTarget::ENEMY
                && (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)) {
                int stacks = is_heavy ? (tr.stacks + 1) : tr.stacks;
                apply_buff(t, tr.buff_id, stacks);
                extra += " " + t->name + get_buff_display_name(tr.buff_id) + "!";
            }
        }
        // D3 Step3 E2: 100%附加 bleed
        if (evolution_level >= 2) { apply_buff(t, "bleed", 1); extra += " 流血!"; }
    }

    int total = 0;
    float base = get_effective_attack(caster) * 1.5f * dmg_bonus;
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

    // D3 Step3 E3: Heavy冲击波 — 向前方直线敌人施加70%二次伤害
    if (evolution_level >= 3 && is_heavy) {
        float cx = caster->entity.rect.x + caster->entity.rect.width/2;
        float cy = caster->entity.rect.y + caster->entity.rect.height/2;
        for (auto* t : targets) {
            if (!t || !t->combat.is_alive) continue;
            float tx = t->entity.rect.x + t->entity.rect.width/2;
            float ty = t->entity.rect.y + t->entity.rect.height/2;
            float dx = tx - cx, dy = ty - cy; float dist = sqrtf(dx*dx+dy*dy);
            if (dist > cr * 48.0f) continue;
            // 检查是否在朝向方向
            bool in_dir = false;
            switch (caster->direction) {
                case Direction::DOWN:  in_dir = (dy > 0  && fabsf(dx) < fabsf(dy)); break;
                case Direction::UP:    in_dir = (dy < 0  && fabsf(dx) < fabsf(dy)); break;
                case Direction::RIGHT: in_dir = (dx > 0  && fabsf(dy) < fabsf(dx)); break;
                case Direction::LEFT:  in_dir = (dx < 0  && fabsf(dy) < fabsf(dx)); break;
            }
            if (in_dir) {
                int wave_dmg = (int)(calculate_damage((int)(base * power * 0.7f),
                    t->combat.get_effective_defense(AttackType::PHYSICAL)));
                t->combat.take_damage(wave_dmg);
                total += wave_dmg;
                extra += " 冲击波!";
            }
        }
    }

    std::string d = (is_heavy ? "重·" : "") + std::string("斩击Lv") + std::to_string(level)
                  + " 命中 " + std::to_string(hit.size()) + " 目标";
    if (hits_per > 1) d += "（二连击）";
    if (!extra.empty()) d += extra;
    return d + "，造成 " + std::to_string(total) + " 伤害";
}

std::string SlashSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " 范围" + std::to_string(_cone_range) + "格 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- FireballSkill ----
FireballSkill::FireballSkill() : ActiveSkill("神罚", 5.0f, 3) {
    triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
    tags = {BuildTag::MAGIC, BuildTag::FIRE, BuildTag::PROJECTILE, BuildTag::AOE, BuildTag::RANGED};
    // D3 Step2: Evolution path
    add_evolution(1, "爆炸火球", "半径+40%", {BuildTag::FIRE, BuildTag::AOE});
    add_evolution(2, "燃烧地面", "3秒燃烧区域持续DOT", {BuildTag::FIRE, BuildTag::DOT});
    add_evolution(3, "分裂火球", "分裂3颗小火球", {BuildTag::FIRE, BuildTag::PROJECTILE, BuildTag::AOE});
}

void FireballSkill::_on_level_up() {
    if (level == 2) { base_cooldown = 4.0f; _target_count = 2; _range = 7.0f; }
    else if (level == 3) { base_cooldown = 3.0f; _target_count = 3; _range = 8.0f; }
    _recalc_cooldown();
}

std::string FireballSkill::execute(Player* caster, std::vector<Monster*>& targets,
                                    GameMap*, bool is_heavy) {
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

    float frng = is_heavy ? _range * 1.5f : _range;
    // D3 Step3 E1: 半径+40%
    if (evolution_level >= 1) frng *= 1.4f;
    float range_px = frng * 32.0f;
    std::vector<Monster*> chosen;
    for (auto* t : alive) {
        if ((int)chosen.size() >= _target_count) break;
        float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - pcx,
                         t->entity.rect.y + t->entity.rect.height/2 - pcy);
        if (d <= range_px) chosen.push_back(t);
    }
    if (chosen.empty()) return is_heavy ? "重·神罚没有击中任何目标" : "神罚没有击中任何目标";

    int total = 0;
    float base = get_effective_attack(caster) * 2.5f;
    float power = get_power_multiplier();
    for (auto* t : chosen) {
        int dmg = calculate_damage((int)(base * power),
            t->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
        t->combat.take_damage(dmg);
        total += dmg;
        if (is_heavy) apply_buff(t, "poison", 2);
        // D3 Step3 E2: 燃烧地面 → 命中目标+poison
        if (evolution_level >= 2) apply_buff(t, "poison", 1);
    }

    // D3 Step3 E3: 分裂火球 — 对所有非主目标敌人施加35%伤害
    if (evolution_level >= 3) {
        int split_count = 0;
        for (auto* t : alive) {
            if (split_count >= 3) break;
            bool already = false;
            for (auto* c : chosen) if (c == t) { already = true; break; }
            if (already) continue;
            float d = hypotf(t->entity.rect.x + t->entity.rect.width/2 - pcx,
                             t->entity.rect.y + t->entity.rect.height/2 - pcy);
            if (d <= range_px * 1.5f) {
                int s_dmg = (int)(calculate_damage((int)(base * power * 0.35f),
                    t->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL));
                t->combat.take_damage(s_dmg);
                total += s_dmg;
                split_count++;
            }
        }
    }

    // Buff 触发规则
    std::string extra;
    if (!chosen.empty()) {
        for (auto& tr : triggers) {
            if (tr.target == BuffTarget::ENEMY
                && (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)) {
                int stacks = is_heavy ? (tr.stacks + 1) : tr.stacks;
                apply_buff(chosen[0], tr.buff_id, stacks);
                extra += " " + chosen[0]->name + get_buff_display_name(tr.buff_id) + "!";
            }
        }
    }
    std::string label = (is_heavy ? "重·" : "") + std::string("神罚Lv") + std::to_string(level);
    return label + " 命中 " + std::to_string(chosen.size())
           + " 目标，造成 " + std::to_string(total) + " 伤害" + extra
           + (is_heavy ? " +燃烧!" : "");
}

std::string FireballSkill::get_level_text() const {
    return "Lv" + std::to_string(level) + " " + std::to_string(_target_count) + "目标 " + std::to_string(cooldown).substr(0,3) + "s";
}

// ---- SelfHealSkill ----
SelfHealSkill::SelfHealSkill() : ActiveSkill("自愈", 8.0f, 3) {
    triggers = {{"attack_up", 1, 1.0f, BuffTarget::SELF}};
    tags = {BuildTag::HEAL, BuildTag::SUPPORT};
    // D3 Step2: Evolution path
    add_evolution(1, "深度自愈", "回血量+50%", {BuildTag::HEAL});
    add_evolution(2, "战意自愈", "回血后attack_up×1", {BuildTag::HEAL, BuildTag::SUPPORT});
    add_evolution(3, "守护自愈", "回血后获得护盾", {BuildTag::HEAL, BuildTag::DEFENSE});
}

void SelfHealSkill::_on_level_up() {
    if (level == 2) base_cooldown = 6.5f;
    else if (level == 3) base_cooldown = 5.0f;
    _recalc_cooldown();
}

std::string SelfHealSkill::execute(Player* caster, std::vector<Monster*>&, GameMap*,
                                     bool is_heavy) {
    float pcts[] = {0, 0.20f, 0.35f, 0.35f};
    float pct = pcts[std::min(level, 3)];
    // D3 Step3 E1: 回血+50%
    if (evolution_level >= 1) pct *= 1.5f;
    float power = get_power_multiplier();
    int instant = (int)(caster->combat.max_hp * pct * power);
    int recovered = caster->combat.heal(instant);
    std::string d = "自愈Lv" + std::to_string(level) + " 瞬回 " + std::to_string(recovered) + " HP";
    if (level >= 3) { _regen_left = 4.0f; d += "（+4秒持续再生）"; }
    apply_triggers_self(triggers, caster);
    if (!triggers.empty()) d += " " + get_buff_display_name(triggers[0].buff_id) + "!";
    // D2 Step2: Heavy → 额外获得 attack_up
    if (is_heavy) { apply_buff(caster, "attack_up", 1); d += " +ATK↑!"; }
    // D3 Step3 E2: 回血后额外attack_up
    if (evolution_level >= 2) { apply_buff(caster, "attack_up", 1); d += " +战意!"; }
    // D3 Step3 E3: 护盾 (数值 = 20% max_hp)
    if (evolution_level >= 3) {
        apply_buff(caster, "shield", 1);
        caster->combat.shield_hp = (float)(caster->combat.max_hp * 0.20f);
        d += " +护盾!";
    }
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
TheWorldSkill::TheWorldSkill() : ActiveSkill("The World", 20.0f, 3) {
    tags = {BuildTag::TIME, BuildTag::SUPPORT};
    // D3 Step2: Evolution path
    add_evolution(1, "延长时间", "停止时间+20%", {BuildTag::TIME});
    add_evolution(2, "时停冲击", "结束时释放Shockwave", {BuildTag::TIME, BuildTag::AOE});
    add_evolution(3, "时间加速", "结束后移速+25% 3秒", {BuildTag::TIME, BuildTag::SUPPORT});
}
void TheWorldSkill::_on_level_up() {
    if (level == 2) base_cooldown = 16.0f;
    else if (level == 3) base_cooldown = 12.0f;
    _recalc_cooldown();
}
float TheWorldSkill::get_stop_duration() const {
    return _stop_duration[std::min(level, 3)] * (1.0f + charm_power_bonus);
}
std::string TheWorldSkill::execute(Player*, std::vector<Monster*>&, GameMap*,
                                    bool is_heavy) {
    (void)is_heavy; // D2 Step2: The World 暂不做 heavy 变化 (已足够强大)
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
