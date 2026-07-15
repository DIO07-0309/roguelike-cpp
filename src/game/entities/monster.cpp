#include "monster.h"
#include "ai.h"
#include "player.h"
#include "combat_system.h"

Monster::Monster(float x, float y, const std::string& n, int hp, int atk,
                 int pdef, int mdef, Color c, MonsterAI* a)
    : entity(x, y, 28, 28), combat(hp, atk, pdef, mdef), name(n), color(c) {
    color.a = 255;
    ai = a ? a : new MonsterAI();
}

Monster::~Monster() { delete ai; }

bool Monster::can_attack(double gt) const {
    return (gt - last_attack_time) >= attack_cooldown;
}

int Monster::attack_target(Player* target, double gt) {
    int dmg = calculate_damage(get_effective_attack(this),
                                target->combat.get_effective_defense(attack_type),
                                attack_type);
    target->combat.take_damage(dmg);
    last_attack_time = (float)gt;
    // 怪物命中附带 Buff (统一触发规则)
    for (auto& tr : on_hit_triggers) {
        if (tr.chance >= 1.0f || (float)(rng() % 1000) / 1000.0f < tr.chance)
            apply_buff(target, tr.buff_id, tr.stacks);
    }
    return dmg;
}

void Monster::update_ai(Player* player, GameMap* map, double dt, double gt,
                         std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (ai) ai->update(this, player, map, dt, gt, all, effects);
}

void Monster::draw(float cam_x, float cam_y) {
    Entity& e = entity;
    Rectangle dr = {e.position.x - cam_x, e.position.y - cam_y,
                    e.size.x, e.size.y};

    // Boss 光晕
    if (is_boss) {
        float pulse = 3 + fabs(sinf((float)GetTime() * 6)) * 5;
        DrawRectangleLinesEx({dr.x - pulse, dr.y - pulse,
                              dr.width + pulse*2, dr.height + pulse*2},
                             3, {60, 5, 5, 255});
    }

    // B14: Bomber 脉冲光晕
    if (monster_type == MonsterType::BOMBER) {
        float pulse = 2 + fabs(sinf((float)GetTime() * 10)) * 3;
        DrawRectangleLinesEx({dr.x - pulse, dr.y - pulse,
                              dr.width + pulse*2, dr.height + pulse*2},
                             2, Color{255, 200, 60, (unsigned char)(120 + pulse * 20)});
    }
    // B14: Tank 厚边框
    if (monster_type == MonsterType::TANK) {
        DrawRectangleLinesEx({dr.x - 2, dr.y - 2, dr.width + 4, dr.height + 4},
                             3, Color{180, 180, 200, 200});
    }
    // D8: Charger — 红色箭头方向指示
    if (monster_type == MonsterType::CHARGER) {
        float cx = dr.x + dr.width/2, cy = dr.y + dr.height/2;
        float arrow = 6 + sinf((float)GetTime() * 8) * 3;
        DrawTriangle({cx + arrow, cy}, {cx - arrow/2, cy - arrow/2},
                     {cx - arrow/2, cy + arrow/2}, Color{255, 100, 40, 255});
    }
    // D8: Summoner — 紫色魔法光环
    if (monster_type == MonsterType::SUMMONER) {
        float ring = 14 + sinf((float)GetTime() * 6) * 3;
        DrawRing({dr.x + dr.width/2, dr.y + dr.height/2}, ring - 2, ring,
                 0, 360, 16, Color{180, 120, 220, 160});
    }

    // 阴影
    DrawEllipse(dr.x + dr.width/2, dr.y + dr.height + 2, dr.width/2 - 2, 3,
                {0, 0, 0, 80});

    // 身体
    DrawRectangleRounded(dr, 0.15f, 4, color);

    // 高光
    DrawRectangleRounded({dr.x + 3, dr.y + 2, dr.width - 6, 6}, 0.1f, 3,
                         Color{(unsigned char)std::min(255u, (unsigned)color.r + 60),
                               (unsigned char)std::min(255u, (unsigned)color.g + 60),
                               (unsigned char)std::min(255u, (unsigned)color.b + 60), 255});

    // 眼睛
    if (name.find("史莱姆") != std::string::npos) {
        float ey = dr.y + dr.height / 2 - 2;
        DrawCircle(dr.x + dr.width / 3, ey, 5, WHITE);
        DrawCircle(dr.x + dr.width * 2 / 3, ey, 5, WHITE);
        DrawCircle(dr.x + dr.width / 3 + 1, ey + 1, 3, {20, 20, 20, 255});
        DrawCircle(dr.x + dr.width * 2 / 3 + 1, ey + 1, 3, {20, 20, 20, 255});
    } else if (name.find("兽人") != std::string::npos) {
        float ey = dr.y + dr.height / 2 - 1;
        for (int sx : {int(dr.x + 5), int(dr.x + dr.width - 9)}) {
            DrawTriangle({(float)sx, ey}, {(float)sx + 5, ey + 2},
                        {(float)sx, ey + 5}, {255, 50, 50, 255});
        }
    }

    // 边框
    Color bc = is_boss ? Color{255, 180, 30, 255} : Color{0, 0, 0, 255};
    DrawRectangleRoundedLines(dr, 0.15f, 4, 2, bc);

    // 血条 (非Boss 且受伤)
    if (!is_boss && combat.current_hp < combat.max_hp) {
        float ratio = (float)combat.current_hp / combat.max_hp;
        DrawRectangle(dr.x, dr.y - 8, dr.width, 4, {40, 10, 10, 255});
        Color hc = ratio > 0.5f ? Color{200, 40, 40, 255} : Color{200, 20, 20, 255};
        DrawRectangle(dr.x, dr.y - 8, dr.width * ratio, 4, hc);
    }
}

Monster* spawn_monster(float px, float py, const std::string& type) {
    // B14: 根据 MonsterType 创建不同定位的怪物
    if (type == "slime") {
        auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1, {100, 180, 100, 255});
        m->monster_type = MonsterType::NORMAL; m->team_role = TeamRole::NONE;
        m->on_hit_triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
        return m;
    }
    if (type == "orc") {
        auto* m = new Monster(px, py, "兽人", 30, 7, 3, 1, {200, 80, 80, 255});
        m->monster_type = MonsterType::NORMAL; m->team_role = TeamRole::NONE;
        m->on_hit_triggers = {{"poison", 1, 0.25f, BuffTarget::ENEMY}};
        return m;
    }
    // B14: 哥布林弓箭手 — 远程低HP, 长视野
    if (type == "archer") {
        auto* m = new Monster(px, py, "哥布林弓箭手", 22, 8, 1, 1, {80, 160, 80, 255});
        m->monster_type = MonsterType::ARCHER; m->team_role = TeamRole::BACKLINE;
        m->attack_cooldown = 2.0f;
        auto* ai = new MonsterAI(8.0f, 70.0f, 3.0f, 6.0f);
        m->ai = ai;
        MonsterSkillState rs = {MonsterSkillType::RAPID_SHOT, 0, 4.0f};
        ai->_skills.push_back(rs);
        return m;
    }
    // B14: 萨满 — 低攻, 辅助
    if (type == "shaman") {
        auto* m = new Monster(px, py, "哥布林萨满", 28, 4, 2, 4, {160, 100, 200, 255});
        m->monster_type = MonsterType::SHAMAN; m->team_role = TeamRole::SUPPORT;
        m->attack_type = AttackType::MAGICAL;
        m->attack_cooldown = 3.0f;
        MonsterSkillState tt = {MonsterSkillType::TOTEM, 3, 10.0f};
        m->ai->_skills.push_back(tt);
        return m;
    }
    // B14: 爆炸怪 — 靠近自爆
    if (type == "bomber") {
        auto* m = new Monster(px, py, "爆炸史莱姆", 25, 0, 2, 2, {255, 140, 40, 255});
        m->monster_type = MonsterType::BOMBER; m->team_role = TeamRole::FLANK;
        m->attack_cooldown = 99.0f;
        auto* ai = new MonsterAI(5.0f, 100.0f, 2.0f, 1.5f);
        m->ai = ai;
        MonsterSkillState lp = {MonsterSkillType::LEAP, 2, 5.0f};
        ai->_skills.push_back(lp);
        return m;
    }
    // B14: 重甲怪 — 高HP低速
    if (type == "tank") {
        auto* m = new Monster(px, py, "重甲兽人", 70, 6, 8, 3, {120, 120, 140, 255});
        m->monster_type = MonsterType::TANK; m->team_role = TeamRole::FRONTLINE;
        auto* ai = new MonsterAI(4.0f, 40.0f, 3.0f, 1.5f);
        m->ai = ai;
        MonsterSkillState sw = {MonsterSkillType::SHIELD, 3, 8.0f};
        ai->_skills.push_back(sw);
        return m;
    }
    // B14: 精英怪 — 强化 + 随机buff + Summon技能
    if (type == "elite") {
        bool is_slime = (rng() % 2 == 0);
        if (is_slime) {
            auto* m = new Monster(px, py, "精英史莱姆", 45, 10, 4, 3, {100, 220, 100, 255});
            m->monster_type = MonsterType::ELITE; m->team_role = TeamRole::COMMAND;
            m->is_elite = true;
            m->on_hit_triggers = {{"slow", 1, 0.40f, BuffTarget::ENEMY}};
            int buff_roll = rng() % 3;
            if (buff_roll == 0) apply_buff(m, "attack_up", 2);
            else if (buff_roll == 1) apply_buff(m, "slow", 1);
            else apply_buff(m, "attack_up", 1);
            MonsterSkillState sm = {MonsterSkillType::SUMMON, 5, 14.0f};
            m->ai->_skills.push_back(sm);
            return m;
        } else {
            auto* m = new Monster(px, py, "精英兽人", 75, 16, 6, 3, {240, 60, 60, 255});
            m->monster_type = MonsterType::ELITE; m->team_role = TeamRole::COMMAND;
            m->is_elite = true;
            m->on_hit_triggers = {{"poison", 2, 0.40f, BuffTarget::ENEMY}};
            int buff_roll = rng() % 3;
            if (buff_roll == 0) apply_buff(m, "attack_up", 3);
            else if (buff_roll == 1) ;
            else apply_buff(m, "attack_up", 1);
            MonsterSkillState sm = {MonsterSkillType::SUMMON, 5, 14.0f};
            m->ai->_skills.push_back(sm);
            return m;
        }
    }
    // D8: 冲锋怪 — 中距蓄力冲刺, 高爆发
    if (type == "charger") {
        auto* m = new Monster(px, py, "冲锋兽人", 40, 10, 4, 2, {200, 140, 60, 255});
        m->monster_type = MonsterType::CHARGER; m->team_role = TeamRole::FRONTLINE;
        m->attack_cooldown = 2.5f;
        auto* ai = new MonsterAI(7.0f, 110.0f, 2.5f, 2.0f);
        m->ai = ai;
        MonsterSkillState ch = {MonsterSkillType::CHARGE, 0, 5.0f};
        ai->_skills.push_back(ch);
        return m;
    }
    // D8: 召唤师 — 低攻, 定期召唤2只小怪
    if (type == "summoner") {
        auto* m = new Monster(px, py, "哥布林召唤师", 35, 5, 3, 5, {180, 120, 220, 255});
        m->monster_type = MonsterType::SUMMONER; m->team_role = TeamRole::SUPPORT;
        m->attack_type = AttackType::MAGICAL;
        m->attack_cooldown = 4.0f;
        auto* ai = new MonsterAI(6.0f, 60.0f, 3.0f, 3.0f);
        m->ai = ai;
        MonsterSkillState ms = {MonsterSkillType::MASS_SUMMON, 3, 12.0f};
        ai->_skills.push_back(ms);
        return m;
    }
    // fallback
    auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1, {100, 180, 100, 255});
    m->monster_type = MonsterType::NORMAL;
    m->on_hit_triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
    return m;
}
