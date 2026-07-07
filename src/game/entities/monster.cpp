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
    if (type == "slime") {
        auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1, {100, 180, 100, 255});
        m->on_hit_triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
        return m;
    } else if (type == "orc") {
        auto* m = new Monster(px, py, "兽人", 30, 7, 3, 1, {200, 80, 80, 255});
        m->on_hit_triggers = {{"poison", 1, 0.25f, BuffTarget::ENEMY}};
        return m;
    }
    auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1, {100, 180, 100, 255});
    m->on_hit_triggers = {{"slow", 1, 0.25f, BuffTarget::ENEMY}};
    return m;
}
