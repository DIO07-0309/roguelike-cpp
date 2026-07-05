#include "boss.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "config.h"

// ---- BossSkill 实现 ----
ConeAttack::ConeAttack() : BossSkill("暗影冲击", 5.0f) {
    fx_radius = 96; fx_color = {200, 40, 40, 255};
}

std::string ConeAttack::execute(Monster* boss, Player* player,
                                 std::vector<Monster*>&, GameMap*, double gt) {
    float bx = boss->entity.rect.x, by = boss->entity.rect.y;
    Rectangle zone = {bx - 64, by - 32, 128, 64};
    if (!CheckCollisionRecs(zone, player->entity.rect))
        return "Boss 释放了 " + name + "，但你没有被打中";

    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * 1.8f),
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    mark_used(gt);
    return "Boss 释放 " + name + "！造成 " + std::to_string(dmg) + " 伤害";
}

CircleAOE::CircleAOE() : BossSkill("地裂", 7.0f) {
    fx_radius = 80; fx_color = {220, 50, 50, 255};
}

std::string CircleAOE::execute(Monster* boss, Player* player,
                                std::vector<Monster*>&, GameMap*, double gt) {
    float dx = (player->entity.rect.x + player->entity.rect.width/2)
             - (boss->entity.rect.x + boss->entity.rect.width/2);
    float dy = (player->entity.rect.y + player->entity.rect.height/2)
             - (boss->entity.rect.y + boss->entity.rect.height/2);
    if (sqrtf(dx*dx + dy*dy) > 80)
        return "Boss 释放了 " + name + "，但你躲开了";

    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * 1.2f),
        player->combat.get_effective_defense(AttackType::MAGICAL),
        AttackType::MAGICAL);
    player->combat.take_damage(dmg);
    mark_used(gt);
    return "Boss 释放 " + name + "！造成 " + std::to_string(dmg) + " 伤害";
}

SummonMinions::SummonMinions() : BossSkill("召唤兽人", 15.0f) {
    fx_radius = 64; fx_color = {150, 50, 200, 255};
}

std::string SummonMinions::execute(Monster* boss, Player*,
                                    std::vector<Monster*>& monsters,
                                    GameMap* map, double gt) {
    int count = 0;
    for (int i = 0; i < 2; i++) {
        int off_x = (int)(rng() % 5) - 2;
        int off_y = (int)(rng() % 5) - 2;
        float sx = boss->entity.position.x + off_x * TILE_SIZE;
        float sy = boss->entity.position.y + off_y * TILE_SIZE;
        auto [tx, ty] = map->pixel_to_tile(sx, sy);
        if (map->is_walkable(tx, ty)) {
            monsters.push_back(spawn_monster(sx, sy, "orc"));
            count++;
        }
    }
    mark_used(gt);
    return "Boss 召唤了 " + std::to_string(count) + " 只兽人！";
}

// ---- BossAI ----
BossAI::BossAI() : MonsterAI(10.0f, 60.0f, 3.0f, 2.0f) {
    skills.push_back(std::make_unique<ConeAttack>());
    skills.push_back(std::make_unique<CircleAOE>());
    skills.push_back(std::make_unique<SummonMinions>());
}

void BossAI::update(Monster* self, Player* player, GameMap* map,
                     double dt, double gt,
                     std::vector<Monster*>* all, std::vector<Effect>* effects) {
    // 狂暴检测
    if (!is_enraged && _hp_ratio(self) < 0.4f) _enter_enrage();

    // 基础 AI
    MonsterAI::update(self, player, map, dt, gt, all, effects);

    // 技能释放
    for (auto& sk : skills) {
        if (sk->can_use(gt)) {
            if (dynamic_cast<SummonMinions*>(sk.get()) &&
                state != AIState::CHASE && state != AIState::ATTACK) continue;

            auto mlist = all ? *all : std::vector<Monster*>{};
            sk->execute(self, player, mlist, map, gt);

            if (effects) {
                VFXServer v;
                float mx = self->entity.rect.x + self->entity.rect.width/2;
                float my = self->entity.rect.y + self->entity.rect.height/2;
                if (dynamic_cast<ConeAttack*>(sk.get()))      v.boss_cone(mx, my);
                else if (dynamic_cast<CircleAOE*>(sk.get()))  v.boss_circle(mx, my);
                else if (dynamic_cast<SummonMinions*>(sk.get())) v.boss_summon(mx, my);
                for (auto& e : v.effects) effects->push_back(e);
            }
            break;
        }
    }
}

void BossAI::_enter_enrage() {
    is_enraged = true;
    move_speed *= 1.6f;
    for (auto& s : skills) s->cooldown *= 0.7f;
}

float BossAI::_hp_ratio(Monster* self) const {
    return (float)self->combat.current_hp / self->combat.max_hp;
}

// ---- 工厂 ----
static BossInfo BOSSES[] = {
    {"暗影骑士", "第一狱守·暗影骑士",
     "曾是王城最荣耀的骑士，被黑暗意志吞噬后\n成为地牢第一道门的永恒守门人。",
     "暗影冲击·地裂·召唤兽人",
     250, 15, 10, 4, {120, 20, 180, 255}},
    {"地狱火魔", "第二狱守·地狱火魔",
     "熔岩深渊中诞生的远古恶魔，\n以灼热烈焰焚烧一切闯入者。",
     "暗影冲击·地裂·召唤兽人",
     500, 24, 14, 8, {240, 100, 20, 255}},
    {"深渊之主·终焉", "终焉·深渊之主",
     "地牢最深处的终极存在，一切黑暗的源头。\n击败他，地牢将重获光明。",
     "暗影冲击·地裂·召唤兽人",
     900, 35, 22, 14, {180, 20, 20, 255}},
};

BossInfo get_boss_info(int floor) {
    int idx = (floor == 5) ? 0 : (floor == 10) ? 1 : 2;
    return BOSSES[idx];
}

Monster* spawn_boss(int tile_x, int tile_y, int floor) {
    auto info = get_boss_info(floor);
    auto* boss = new Monster(
        (float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
        info.name, info.max_hp, info.attack, info.pdef, info.mdef,
        info.color, new BossAI());
    boss->is_boss = true;
    boss->entity.size = {48, 48};
    boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};
    return boss;
}
