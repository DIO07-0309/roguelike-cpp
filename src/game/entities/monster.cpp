#include "monster.h"
#include "ai.h"
#include "player.h"
#include "combat_system.h"
#include "data/enemy_defs.h"   // G1 Step5

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

// ============================================================
// String → Enum helpers (gameplay logic, stays in entity layer)
// ============================================================

static MonsterType _str_to_monster_type(const std::string& s) {
    if (s == "archer")   return MonsterType::ARCHER;
    if (s == "shaman")   return MonsterType::SHAMAN;
    if (s == "bomber")   return MonsterType::BOMBER;
    if (s == "tank")     return MonsterType::TANK;
    if (s == "elite")    return MonsterType::ELITE;
    if (s == "charger")  return MonsterType::CHARGER;
    if (s == "summoner") return MonsterType::SUMMONER;
    return MonsterType::NORMAL;
}

static TeamRole _str_to_team_role(const std::string& s) {
    if (s == "frontline") return TeamRole::FRONTLINE;
    if (s == "backline")  return TeamRole::BACKLINE;
    if (s == "support")   return TeamRole::SUPPORT;
    if (s == "flank")     return TeamRole::FLANK;
    if (s == "command")   return TeamRole::COMMAND;
    return TeamRole::NONE;
}

static AttackType _str_to_attack_type(const std::string& s) {
    if (s == "magical") return AttackType::MAGICAL;
    if (s == "true")    return AttackType::TRUE;
    return AttackType::PHYSICAL;
}

static MonsterSkillType _str_to_skill_type(const std::string& s) {
    if (s == "rapid_shot")   return MonsterSkillType::RAPID_SHOT;
    if (s == "totem")        return MonsterSkillType::TOTEM;
    if (s == "leap")         return MonsterSkillType::LEAP;
    if (s == "shield")       return MonsterSkillType::SHIELD;
    if (s == "summon")       return MonsterSkillType::SUMMON;
    if (s == "charge")       return MonsterSkillType::CHARGE;
    if (s == "mass_summon")  return MonsterSkillType::MASS_SUMMON;
    if (s == "snipe")        return MonsterSkillType::SNIPE;
    if (s == "scatter")      return MonsterSkillType::SCATTER;
    if (s == "ambush_attack") return MonsterSkillType::AMBUSH_ATTACK;
    if (s == "guard_aura")   return MonsterSkillType::GUARD_AURA;
    return MonsterSkillType::NONE;
}

static AIArchetype _str_to_archetype(const std::string& s) {
    if (s == "bomber" || s == "charger")   // old monster_type mapping fallback
    { if (s == "bomber")   return AIArchetype::BOMBER;
      if (s == "charger")  return AIArchetype::CHARGER; }
    if (s == "shaman")    return AIArchetype::SHAMAN;
    if (s == "sniper")    return AIArchetype::SNIPER;
    if (s == "controller")return AIArchetype::CONTROLLER;
    if (s == "ambush")    return AIArchetype::AMBUSH;
    if (s == "guardian")  return AIArchetype::GUARDIAN;
    if (s == "summoner")  return AIArchetype::SUMMONER;
    return AIArchetype::DEFAULT;
}

// visual_id → Color (表现层映射, 未来替换为 texture/animation)
static Color _visual_to_color(const std::string& vid) {
    if (vid == "slime")       return {100, 180, 100, 255};
    if (vid == "orc")         return {200,  80,  80, 255};
    if (vid == "archer")      return { 80, 160,  80, 255};
    if (vid == "shaman")      return {160, 100, 200, 255};
    if (vid == "bomber")      return {255, 140,  40, 255};
    if (vid == "tank")        return {120, 120, 140, 255};
    if (vid == "elite_slime") return {100, 220, 100, 255};
    if (vid == "elite_orc")   return {240,  60,  60, 255};
    if (vid == "charger")     return {200, 140,  60, 255};
    if (vid == "summoner")    return {180, 120, 220, 255};
    return {100, 180, 100, 255}; // fallback: slime green
}

// ============================================================
// G1 Step5: spawn_monster — generic factory driven by EnemyDef
// 新增普通怪物只需修改 enemies.json, 无需改 C++ 代码
// ============================================================
Monster* spawn_monster(float px, float py, const std::string& type) {
    // Elite: 随机选择变体 (唯一的运行时分支逻辑)
    std::string lookup = type;
    if (type == "elite") {
        lookup = (rng() % 2 == 0) ? "elite_slime" : "elite_orc";
    }

    const EnemyDef* def = get_enemy_def(lookup);
    if (!def) {
        // fallback — 配置缺失时安全降级
        auto* m = new Monster(px, py, "史莱姆", 15, 3, 0, 1,
                              _visual_to_color("slime"));
        m->monster_type = MonsterType::NORMAL;
        m->team_role = TeamRole::NONE;
        return m;
    }

    // 创建 AI (使用嵌套 ai 配置)
    auto* ai = new MonsterAI(def->ai.sight, def->ai.speed,
                             def->ai.patrol, def->ai.attack_range);

    // 创建 Monster (visual_id → Color 映射在 _visual_to_color)
    auto* m = new Monster(px, py, def->name, def->hp, def->atk,
                          def->pdef, def->mdef,
                          _visual_to_color(def->visual_id), ai);

    // 枚举字段 (字符串 → enum 映射)
    m->monster_type    = _str_to_monster_type(def->type_str);
    m->team_role       = _str_to_team_role(def->role_str);
    m->attack_type     = _str_to_attack_type(def->attack_type_str);
    m->attack_cooldown = def->attack_cooldown;
    m->is_elite        = def->is_elite;

    // G5.3: AI Archetype (行为原型, 与 MonsterType 外观解耦)
    if (ai) ai->archetype = _str_to_archetype(def->ai_archetype);

    // on_hit 触发器 (直接拷贝)
    m->on_hit_triggers = def->on_hit;

    // 技能 (数据驱动, 每条 record → MonsterSkillState)
    for (auto& sk : def->skills) {
        MonsterSkillType st = _str_to_skill_type(sk.id);
        if (st == MonsterSkillType::NONE) continue;
        MonsterSkillState state = {st, sk.initial_cooldown, sk.max_cooldown};
        ai->_skills.push_back(state);
    }

    // 精英: 随机 Buff (从 buff_pool 中抽一条, 空 buff_id = 跳过)
    if (def->is_elite && !def->elite_buff_pool.empty()) {
        int roll = (int)(rng() % (uint32_t)def->elite_buff_pool.size());
        auto& eb = def->elite_buff_pool[roll];
        if (!eb.buff_id.empty())
            apply_buff(m, eb.buff_id, eb.stacks);
    }

    return m;
}
