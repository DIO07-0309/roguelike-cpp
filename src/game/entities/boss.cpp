#include "boss.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "config.h"
#include "core/logger.h"
#include <cmath>

// ---- BossSkill 基类 ----
BossSkill::BossSkill(const std::string& n, float cd) : name(n), cooldown(cd) {}
bool BossSkill::can_use(double t) const { return (t - last_use_time) >= cooldown; }
void BossSkill::mark_used(double t) { last_use_time = t; }

// ============================================================
// B15: ChargeSkill — 锁定玩家, 蓄力→冲刺
// ============================================================
ChargeSkill::ChargeSkill() : BossSkill("冲锋", 6.0f) {
    fx_kind = "cone"; fx_radius = 120; fx_color = {255, 80, 40, 255};
}

std::string ChargeSkill::execute(Monster* boss, Player* player,
                                  std::vector<Monster*>&, GameMap* map, double) {
    if (windup_left > 0) {
        // 蓄力中：身体变红预警
        boss->color = Color{255, 60, 40, 255};
        return "";
    }
    if (dash_duration > 0) {
        // 冲刺中
        float speed = 500.0f * 0.016f;  // per-frame-ish step
        for (int step = 0; step < 4; step++) {
            boss->entity.position.x += dash_dx * speed;
            boss->entity.position.y += dash_dy * speed;
            boss->entity.sync_rect();
            // 撞墙停止
            if (!map->is_rect_walkable(boss->entity.rect)) {
                boss->entity.position.x -= dash_dx * speed;
                boss->entity.position.y -= dash_dy * speed;
                boss->entity.sync_rect();
                dash_duration = 0;
                break;
            }
            // 撞到玩家
            Rectangle br = boss->entity.rect;
            Rectangle pr = player->entity.rect;
            if (CheckCollisionRecs(br, pr)) {
                int dmg = calculate_damage(
                    (int)(boss->combat.get_effective_attack() * 2.5f),
                    player->combat.get_effective_defense(AttackType::PHYSICAL));
                player->combat.take_damage(dmg);
                dash_duration = 0;
                mark_used(GetTime());
                boss->color = Color{200, 40, 40, 255};
                return "Boss 冲锋命中！造成 " + std::to_string(dmg) + " 伤害";
            }
        }
        return "";
    }
    return "";
}

// ============================================================
// B15: ShockwaveSkill — 蓄力→周围AOE
// ============================================================
ShockwaveSkill::ShockwaveSkill() : BossSkill("冲击波", 8.0f) {
    fx_kind = "circle"; fx_radius = 100; fx_color = {255, 180, 30, 255};
}

std::string ShockwaveSkill::execute(Monster* boss, Player* player,
                                     std::vector<Monster*>&, GameMap*, double) {
    if (windup_left > 0) {
        // 蓄力中：闪烁
        float flicker = sinf((float)GetTime() * 20);
        boss->color = Color{255, 200, (unsigned char)(60 + (int)(flicker * 60)), 255};
        return "";
    }
    // 释放
    float dx = (player->entity.rect.x + player->entity.rect.width/2)
             - (boss->entity.rect.x + boss->entity.rect.width/2);
    float dy = (player->entity.rect.y + player->entity.rect.height/2)
             - (boss->entity.rect.y + boss->entity.rect.height/2);
    float dist = sqrtf(dx*dx + dy*dy);
    mark_used(GetTime());
    boss->color = Color{200, 40, 40, 255};
    if (dist > 100) return "Boss 释放冲击波，但你躲开了";
    int dmg = calculate_damage(
        (int)(boss->combat.get_effective_attack() * 1.6f),
        player->combat.get_effective_defense(AttackType::MAGICAL),
        AttackType::MAGICAL);
    player->combat.take_damage(dmg);
    return "Boss 冲击波！造成 " + std::to_string(dmg) + " 伤害";
}

// ============================================================
// SummonMinions (B7 保留, B15 加入固定循环)
// ============================================================
SummonMinions::SummonMinions() : BossSkill("召唤手下", 12.0f) {
    fx_kind = "circle"; fx_radius = 80; fx_color = {100, 200, 100, 255};
}

std::string SummonMinions::execute(Monster* boss, Player*,
                                    std::vector<Monster*>& monsters,
                                    GameMap* map, double) {
    int count = 0;
    for (int i = 0; i < 3; i++) {
        int off_x = (int)(rng() % 7) - 3;
        int off_y = (int)(rng() % 7) - 3;
        float sx = boss->entity.position.x + off_x * TILE_SIZE;
        float sy = boss->entity.position.y + off_y * TILE_SIZE;
        auto [tx, ty] = map->pixel_to_tile(sx, sy);
        if (map->is_walkable(tx, ty)) {
            const char* type = (rng() % 3 == 0) ? "orc" : "slime";
            auto* m = spawn_monster(sx, sy, type);
            if (m) { monsters.push_back(m); count++; }
        }
    }
    mark_used(GetTime());
    if (count > 0)
        return "Boss 召唤了 " + std::to_string(count) + " 只手下！";
    return "Boss 召唤失败（无空间）";
}

// ============================================================
// BossAI — B15: 技能循环状态机 + Phase 2
// ============================================================
BossAI::BossAI() : MonsterAI(10.0f, 60.0f, 3.0f, 2.0f) {
    _charge    = std::make_unique<ChargeSkill>();
    _shockwave = std::make_unique<ShockwaveSkill>();
    _summon    = std::make_unique<SummonMinions>();
}

int BossAI::_next_cycle_skill() {
    // D8: skill_cycle_bias defines pattern: 6=normal, 4=summon-heavy
    int cycle_len = skill_cycle_bias;
    int idx = skill_cycle_index % cycle_len;
    skill_cycle_index++;
    if (idx == 0) return 0;       // Charge
    if (idx == 2) return 1;       // Shockwave
    if (cycle_len == 4 && idx == 3) return 2; // Necromancer: Summon at idx 3 (every 2nd after norm)
    if (cycle_len == 6 && idx == 4) return 2; // Normal: Summon at idx 4
    return -1;                     // 普攻
}

void BossAI::update(Monster* self, Player* player, GameMap* map,
                     double dt, double gt,
                     std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (!self->combat.is_alive) return;

    // B15: Phase 2 检测 (仅触发一次, HP < 50%)
    if (!phase2 && _hp_ratio(self) < 0.5f) {
        _enter_phase2(self);
        return; // 本帧暂停
    }

    // Phase 2 暂停计时
    if (phase2_pause > 0) {
        phase2_pause -= (float)dt;
        return; // 暂停中
    }

    // ── B15: Boss 状态机 ──
    _tick_boss_state(self, player, map, dt, gt, all, effects);
}

void BossAI::_enter_phase2(Monster* self) {
    phase2 = true;
    phase2_pause = 0.5f;
    is_enraged = true;
    move_speed *= 1.5f;
    self->attack_cooldown *= 0.7f;
    self->combat.attack = (int)(self->combat.attack * 1.25f);
    self->entity.size = {52, 52};  // visually bigger
    self->entity.sync_rect();
    LOG_INFO("[BOSS] Phase 2 触发! 攻击+25%% 移速+50%%");
}

static void _spawn_boss_vfx(Monster* self, const std::string& kind,
                             std::vector<Effect>* effects) {
    if (!effects) return;
    VFXServer v;
    float mx = self->entity.rect.x + self->entity.rect.width / 2;
    float my = self->entity.rect.y + self->entity.rect.height / 2;
    if (kind == "charge")       v.boss_cone(mx, my);
    else if (kind == "shockwave") v.boss_circle(mx, my);
    else if (kind == "summon")    v.boss_summon(mx, my);
    for (auto& e : v.effects) effects->push_back(e);
}

void BossAI::_tick_boss_state(Monster* self, Player* player, GameMap* map,
                               double dt, double gt,
                               std::vector<Monster*>* all, std::vector<Effect>* effects) {
    switch (boss_state) {
    case BossState::IDLE: {
        // 基础 AI (追逐/巡逻)
        MonsterAI::update(self, player, map, dt, gt, all, effects);
        // 检查是否进入ATTACK范围 → 切换为技能循环
        float dist = _dist_to(self, player);
        if (dist <= attack_range * 32.0f) {
            boss_state = BossState::ATTACK;
            normal_attack_count = 0;
            skill_cycle_index = 0;
        }
        break;
    }
    case BossState::ATTACK: {
        float dist = _dist_to(self, player);
        if (dist > sight_range * 32.0f) {
            boss_state = BossState::IDLE;
            break;
        }
        // 先执行普攻
        if (self->can_attack(gt)) {
            self->attack_target(player, gt);
            _spawn_boss_vfx(self, "charge", effects); // reuse vfx for norm attack
            normal_attack_count++;
        }
        // 普攻2次后 → 使用下个技能 (D8: Golem adds DEFEND to cycle)
        if (normal_attack_count >= 2) {
            int sk = _next_cycle_skill();
            // D8: Golem — every other cycle, use DEFEND instead of Shockwave
            if (golem_shield_pct > 0 && sk == 1 && (skill_cycle_index % 12) < 6) {
                sk = 3; // override: DEFEND instead of Shockwave
            }
            if (sk == 0) {
                boss_state = BossState::CHARGE;
                _charge->windup_left = 0.6f;
                _charge->dash_duration = 0.0f;
                _spawn_boss_vfx(self, "charge", effects);
            } else if (sk == 1) {
                boss_state = BossState::SHOCKWAVE;
                _shockwave->windup_left = 0.7f;
                _spawn_boss_vfx(self, "shockwave", effects);
            } else if (sk == 2) {
                boss_state = BossState::SUMMON;
                _spawn_boss_vfx(self, "summon", effects);
            } else if (sk == 3) {
                boss_state = BossState::DEFEND;
                _spawn_boss_vfx(self, "shockwave", effects); // reuse VFX
            }
            normal_attack_count = 0;
        }
        // 仍然追向玩家
        if (dist > attack_range * 32.0f) {
            float dx = player->entity.rect.x + player->entity.rect.width/2
                     - self->entity.rect.x - self->entity.rect.width/2;
            float dy = player->entity.rect.y + player->entity.rect.height/2
                     - self->entity.rect.y - self->entity.rect.height/2;
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt);
        }
        break;
    }
    case BossState::CHARGE: {
        // 蓄力阶段 (C1: 红色预警圈)
        if (_charge->windup_left > 0) {
            _charge->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = 60; warn.duration = 0.1f; warn.elapsed = 0;
                warn.color = {255, 40, 30, 180};
                effects->push_back(warn);
            }
            { std::vector<Monster*> dummy; _charge->execute(self, player, dummy, map, gt); }
            if (_charge->windup_left <= 0) {
                // 蓄力结束 → 开始冲刺
                float dx = player->entity.rect.x + player->entity.rect.width/2
                         - self->entity.rect.x - self->entity.rect.width/2;
                float dy = player->entity.rect.y + player->entity.rect.height/2
                         - self->entity.rect.y - self->entity.rect.height/2;
                float len = sqrtf(dx*dx + dy*dy);
                if (len > 0) { _charge->dash_dx = dx/len; _charge->dash_dy = dy/len; }
                _charge->dash_duration = 0.3f;
            }
            break;
        }
        // 冲刺阶段
        if (_charge->dash_duration > 0) {
            _charge->dash_duration -= (float)dt;
            std::vector<Monster*> dm; (void)dm;
            std::string result = _charge->execute(self, player, dm, map, gt);
            if (!result.empty() && result.find("命中") != std::string::npos) {
                // 命中玩家, 恢复
                boss_state = BossState::ATTACK;
                normal_attack_count = 0;
            }
            if (_charge->dash_duration <= 0) {
                self->color = Color{200, 40, 40, 255};
                _charge->mark_used(gt);
                boss_state = BossState::ATTACK;
                normal_attack_count = 0;
            }
            break;
        }
        boss_state = BossState::ATTACK;
        normal_attack_count = 0;
        break;
    }
    case BossState::SHOCKWAVE: {
        // C1: 蓄力阶段显示半透明冲击波范围
        if (_shockwave->windup_left > 0) {
            _shockwave->windup_left -= (float)dt;
            if (effects) {
                Effect warn;
                warn.kind = "pulse";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.radius = 100; warn.duration = 0.15f; warn.elapsed = 0;
                warn.color = {255, 200, 50, 130};
                effects->push_back(warn);
            }
            std::vector<Monster*> dm2;
            _shockwave->execute(self, player, dm2, map, gt);
            if (_shockwave->windup_left <= 0) {
                // 释放
                _spawn_boss_vfx(self, "shockwave", effects);
                { std::vector<Monster*> dm3; _shockwave->execute(self, player, dm3, map, gt); }
                self->color = Color{200, 40, 40, 255};
                _shockwave->mark_used(gt);
                boss_state = BossState::ATTACK;
                normal_attack_count = 0;
            }
            break;
        }
        boss_state = BossState::ATTACK;
        normal_attack_count = 0;
        break;
    }
    case BossState::SUMMON: {
        auto mlist = all ? *all : std::vector<Monster*>{};
        // D8: Necromancer Phase2 summons more
        int extra = (phase2 && skill_cycle_bias == 4) ? 2 : 0;
        for (int n = 0; n < 1 + extra; n++) {
            _summon->execute(self, player, mlist, map, gt);
        }
        _spawn_boss_vfx(self, "summon", effects);
        boss_state = BossState::ATTACK;
        normal_attack_count = 0;
        break;
    }
    case BossState::DEFEND: {
        // D8: Golem — 举盾减伤 3 秒
        auto& mod = self->combat.modifiers;
        float pct = golem_shield_pct > 0 ? golem_shield_pct : 0.70f;
        mod["def_pct"] = (mod.count("def_pct") ? mod["def_pct"] : 0) + pct;
        if (effects) {
            Effect s;
            s.kind = "pulse";
            s.world_x = self->entity.rect.x + self->entity.rect.width/2;
            s.world_y = self->entity.rect.y + self->entity.rect.height/2;
            s.radius = 56; s.duration = 0.5f; s.elapsed = 0;
            s.color = {60, 140, 255, 160};
            effects->push_back(s);
        }
        boss_state = BossState::ATTACK;
        normal_attack_count = 0;
        break;
    }
    default: break;
    }
}

float BossAI::_hp_ratio(Monster* self) const {
    return (float)self->combat.current_hp / self->combat.max_hp;
}

// ---- BossInfo 表 (B7 compat) ----
static BossInfo BOSSES[] = {
    {"暗影骑士", "第一狱守·暗影骑士",
     "曾是王城最荣耀的骑士，被黑暗吞噬后\n成为地牢第一道门的永恒守门人。",
     "冲锋·冲击波·召唤手下",
     250, 15, 10, 4, {120, 20, 180, 255}},
    {"地狱火魔", "第二狱守·地狱火魔",
     "熔岩深渊中诞生的远古恶魔，\n以灼热烈焰焚烧一切闯入者。",
     "冲锋·冲击波·召唤手下",
     500, 24, 14, 8, {240, 100, 20, 255}},
    {"深渊之主·终焉", "终焉·深渊之主",
     "地牢最深处的终极存在，一切黑暗的源头。\n击败他，地牢将重获光明。",
     "冲锋·冲击波·召唤手下",
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

// ============================================================
// D8 Step2: BossFactory — 按类型创建 + 配置驱动
// ============================================================
static const BossTemplate BOSS_TEMPLATES[] = {
    // type             floor name         title                    lore
    {BossType::SHADOW_KNIGHT,5,"暗影骑士","第一狱守·暗影骑士","曾是王城最荣耀的骑士，被黑暗吞噬后\n成为地牢第一道门的永恒守门人。",
     false,false, 250,15,10,4, {120,20,180,255}, 1.0f,0.0f},
    {BossType::NECROMANCER, 5,"亡灵法师","第一狱守·亡灵法师","从墓地中召唤亡灵的黑暗法师，\n让你感受到死亡从未真正的离开。",
     true, false, 240,14, 9,7, {80,180,80,255},   2.0f,0.0f},  // D9: HP/ATK closer to Shadow Knight
    {BossType::FIRE_DEMON, 10,"地狱火魔","第二狱守·地狱火魔","熔岩深渊中诞生的远古恶魔，\n以灼热烈焰焚烧一切闯入者。",
     false,false, 500,24,14,8, {240,100,20,255},  1.0f,0.0f},
    {BossType::GOLEM,      10,"远古魔像","第二狱守·远古魔像","由熔岩冷却后的黑曜石组成，\n坚不可摧的魔法造物。",
     false,true,  520,23,18,13,{100,100,130,255}, 1.0f, 0.60f},  // D9: DEFEND减60%(was70%), ATK closer to FireDemon
    {BossType::DEMON_LORD, 15,"深渊之主·终焉","终焉·深渊之主","地牢最深处的终极存在，一切黑暗的源头。\n击败他，地牢将重获光明。",
     false,false, 900,35,22,14,{180,20,20,255},   1.0f,0.0f},
};

const BossTemplate* boss_factory_lookup(BossType type) {
    for (auto& t : BOSS_TEMPLATES)
        if (t.type == type) return &t;
    return &BOSS_TEMPLATES[0];
}

BossType boss_type_for_floor(int floor, uint32_t seed) {
    // D8: 随机Boss池 — 当前简化实现, 后续可加权重
    auto rng_local = std::mt19937(seed ? seed : (uint32_t)std::random_device{}());
    if (floor == 5) {
        return (rng_local() % 2 == 0) ? BossType::SHADOW_KNIGHT : BossType::NECROMANCER;
    } else if (floor == 10) {
        return (rng_local() % 2 == 0) ? BossType::FIRE_DEMON : BossType::GOLEM;
    }
    return BossType::DEMON_LORD; // F15 固定
}

Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters, GameMap* map) {
    const BossTemplate* tmpl = boss_factory_lookup(type);
    BossAI* ai = new BossAI();
    Monster* boss = new Monster(
        (float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
        tmpl->name, tmpl->max_hp, tmpl->attack, tmpl->pdef, tmpl->mdef,
        tmpl->color, ai);
    boss->is_boss = true;
    boss->entity.size = {48, 48};
    boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};

    // D8: Necromancer — 调整技能循环 (SUMMON more frequent)
    if (tmpl->is_summoner) {
        ai->_summon->cooldown = 6.0f;  // faster
        ai->skill_cycle_bias = 4;       // SUMMON every 2 norm attacks + bias
    }
    // D8: Golem — DEFEND state
    if (tmpl->is_defender) {
        ai->golem_shield_pct = tmpl->shield_pct;
    }

    // D4.6: 自动记录世界标记 (Boss击败后由 FlowDirector 设置)
    (void)out_monsters; (void)map;
    return boss;
}
