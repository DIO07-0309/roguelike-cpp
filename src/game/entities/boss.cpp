#include "boss.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "config.h"
#include "core/logger.h"
#include "data/boss_defs.h"      // G1 Step6
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

// ═══════════════════════════════════════════════════════════════
// G5.4: WhirlwindSkill — 360°旋转斩 (Shadow Knight Phase2)
// ═══════════════════════════════════════════════════════════════
WhirlwindSkill::WhirlwindSkill() : BossSkill("旋风斩", 10.0f) {
    fx_kind = "circle"; fx_radius = 140; fx_color = {180, 20, 200, 255};
}
std::string WhirlwindSkill::execute(Monster* boss, Player* player,
    std::vector<Monster*>&, GameMap*, double) {
    if (spin_duration > 0) {
        int dmg = calculate_damage((int)(boss->combat.get_effective_attack() * 1.3),
            player->combat.get_effective_defense(AttackType::PHYSICAL));
        player->combat.take_damage(dmg);
        spin_hit_count++;
        if (spin_duration <= 0.3f) {
            mark_used(GetTime());
            return "旋风斩结束! " + std::to_string(spin_hit_count) + " hits";
        }
        return "";
    }
    return "";
}

// G5.4: LaserBarrageSkill — 3-way 远程贯穿弹 (Fire Demon Phase2)
LaserBarrageSkill::LaserBarrageSkill() : BossSkill("炼狱激光", 9.0f) {
    fx_kind = "cone"; fx_radius = 200; fx_color = {255, 100, 20, 255};
}
std::string LaserBarrageSkill::execute(Monster* boss, Player* player,
    std::vector<Monster*>&, GameMap*, double) {
    if (windup_left > 0) { return ""; } // 蓄力中
    float bx = boss->entity.rect.x + boss->entity.rect.width/2;
    float by = boss->entity.rect.y + boss->entity.rect.height/2;
    float dx = player->entity.rect.x + player->entity.rect.width/2 - bx;
    float dy = player->entity.rect.y + player->entity.rect.height/2 - by;
    float base_ang = atan2f(dy, dx);
    int total = 0;
    for (int i = -1; i <= 1; i++) {
        float ang = base_ang + (float)i * 0.25f; // ±15° spread
        float lx = cosf(ang), ly = sinf(ang);
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float dot = (px-bx)*lx + (py-by)*ly;
        float perp = fabsf((px-bx)*(-ly) + (py-by)*lx);
        if (perp < 60.0f && dot > 0 && dot < 300.0f) {
            int dmg = calculate_damage((int)(boss->combat.get_effective_attack() * 1.8),
                player->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
            player->combat.take_damage(dmg); total += dmg;
        }
    }
    mark_used(GetTime());
    return total > 0 ? "激光弹幕 造成 " + std::to_string(total) + " 伤害" : "激光未命中";
}

// ============================================================
// BossAI — B15: 技能循环状态机 + Phase 2
// ============================================================
BossAI::BossAI() : MonsterAI(10.0f, 60.0f, 3.0f, 2.0f) {
    _charge    = std::make_unique<ChargeSkill>();
    _shockwave = std::make_unique<ShockwaveSkill>();
    _summon    = std::make_unique<SummonMinions>();
    _whirlwind = std::make_unique<WhirlwindSkill>();  // G5.4
    _laser     = std::make_unique<LaserBarrageSkill>();// G5.4
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

    // B15: Phase 2 检测 (仅触发一次, HP < 阈值来自 BossDef)
    if (!phase2 && _hp_ratio(self) < _phase2_hp_threshold) {
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
    phase2_pause = _phase2_pause;                       // G1 Step6: from BossDef
    is_enraged = true;
    move_speed *= _phase2_speed_mult;                   // G1 Step6: from BossDef
    self->attack_cooldown *= _phase2_cd_mult;           // G1 Step6: from BossDef
    self->combat.attack = (int)(self->combat.attack * _phase2_atk_mult); // G1 Step6
    self->entity.size = {52, 52};  // visually bigger
    self->entity.sync_rect();
    LOG_INFO("[BOSS] Phase 2 触发! 攻击+%.0f%% 移速+%.0f%%",
             (_phase2_atk_mult - 1.0f) * 100, (_phase2_speed_mult - 1.0f) * 100);
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
            // ── G5.4: Phase2 signature skill injection ──
            if (phase2 && _boss_id) {
                // Shadow Knight: every 3rd cycle → Whirlwind
                if (strcmp(_boss_id,"shadow_knight")==0 && (skill_cycle_index%9)<3) sk=4;
                // Fire Demon: every 3rd cycle → Laser Barrage
                else if (strcmp(_boss_id,"fire_demon")==0 && (skill_cycle_index%9)<3) sk=5;
                // Demon Lord: every 4th cycle → Gravity Pull (shockwave with 2x range)
                else if (strcmp(_boss_id,"demon_lord")==0 && (skill_cycle_index%12)<3) sk=6;
                // Vampire: every cycle → faster charge+shockwave (lifesteal on hit)
                else if (strcmp(_boss_id,"vampire")==0) {
                    self->attack_cooldown *= 0.85f;
                    if (sk==0) _charge->windup_left *= 0.7f;
                }
                // Necromancer: summoned adds get Guardian buff in Phase2
                else if (strcmp(_boss_id,"necromancer")==0 && sk==2 && all) {
                    for (auto* m : *all) if (m && !m->is_boss && m->combat.is_alive)
                        apply_buff(m,"defense_up",1);
                }
                // Golem: Phase2 → consecutive shockwaves (3 waves)
                else if (strcmp(_boss_id,"golem")==0 && sk==1) {
                    _shockwave->windup_left = 0.7f;
                    // spawn 2 more on next cycle via state
                }
            }
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
            } else if (sk == 4) {
                boss_state = BossState::WHIRLWIND;
                _spawn_boss_vfx(self, "charge", effects);
            } else if (sk == 5) {
                boss_state = BossState::LASER_BARRAGE;
                _laser->windup_left = 0.8f;
                _spawn_boss_vfx(self, "shockwave", effects);
            } else if (sk == 6) {
                boss_state = BossState::GRAVITY_PULL;
                _spawn_boss_vfx(self, "charge", effects);
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
    // ── G5.4: Signature Phase2 skill states ──
    case BossState::WHIRLWIND: {
        if (_whirlwind->spin_duration <= 0) {
            _whirlwind->spin_duration = 1.2f;
            _whirlwind->spin_hit_count = 0;
            _whirlwind->windup_left = 0.4f; // 蓄力预警
        }
        if (_whirlwind->windup_left > 0) {
            _whirlwind->windup_left -= (float)dt;
            _spawn_boss_vfx(self, "charge", effects); // 预警特效
            break;
        }
        _whirlwind->spin_duration -= (float)dt;
        _whirlwind->execute(self, player, {}, map, gt);
        // boss slowly moves toward player while spinning
        float dx = player->entity.rect.x + player->entity.rect.width/2
                 - self->entity.rect.x - self->entity.rect.width/2;
        float dy = player->entity.rect.y + player->entity.rect.height/2
                 - self->entity.rect.y - self->entity.rect.height/2;
        float len = sqrtf(dx*dx+dy*dy);
        if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt * 0.6);
        if (_whirlwind->spin_duration <= 0) {
            boss_state = BossState::ATTACK; normal_attack_count = 0;
        }
        break;
    }
    case BossState::LASER_BARRAGE: {
        if (_laser->windup_left > 0) {
            _laser->windup_left -= (float)dt;
            _spawn_boss_vfx(self, "shockwave", effects); // 蓄力预警
            if (_laser->windup_left <= 0) {
                _laser->execute(self, player, {}, map, gt);
                _spawn_boss_vfx(self, "shockwave", effects);
                boss_state = BossState::ATTACK; normal_attack_count = 0;
            }
        } else {
            _laser->windup_left = 0.8f;
        }
        break;
    }
    case BossState::GRAVITY_PULL: {
        // Demon Lord: pull player closer + delayed shockwave
        float bx = self->entity.rect.x + self->entity.rect.width/2;
        float by = self->entity.rect.y + self->entity.rect.height/2;
        float px = player->entity.rect.x + player->entity.rect.width/2;
        float py = player->entity.rect.y + player->entity.rect.height/2;
        float dx = bx - px, dy = by - py;
        float len = sqrtf(dx*dx+dy*dy);
        if (len > 1 && len < 300.0f) {
            player->entity.position.x += dx/len * 120.0f * (float)dt;
            player->entity.position.y += dy/len * 120.0f * (float)dt;
            player->entity.sync_rect();
        }
        // 0.8s pull → shockwave with 1.5x range
        static float _grav_timer = 0;
        _grav_timer += (float)dt;
        if (_grav_timer > 0.8f) {
            _grav_timer = 0;
            _shockwave->fx_radius *= 1.5f;
            _shockwave->execute(self, player, {}, map, gt);
            _shockwave->fx_radius /= 1.5f;
            _shockwave->mark_used(gt);
            _spawn_boss_vfx(self, "shockwave", effects);
            boss_state = BossState::ATTACK; normal_attack_count = 0;
        }
        break;
    }
    default: break;
    }
}

float BossAI::_hp_ratio(Monster* self) const {
    return (float)self->combat.current_hp / self->combat.max_hp;
}

// ============================================================
// G1 Step6: visual_id → Color 映射 (表现层, 未来替换为 texture)
// ============================================================
Color get_boss_visual_color(const std::string& vid) {
    if (vid == "shadow_knight") return {120, 20, 180, 255};
    if (vid == "necromancer")   return {80, 180, 80, 255};
    if (vid == "vampire")       return {180, 40, 60, 255};
    if (vid == "fire_demon")    return {240, 100, 20, 255};
    if (vid == "golem")         return {100, 100, 130, 255};
    if (vid == "demon_lord")    return {180, 20, 20, 255};
    return {180, 20, 20, 255};  // fallback: demon lord red
}

// ============================================================
// G1 Step6: BossType → BossDef::id 辅助映射
// ============================================================
static const char* _boss_type_to_id(BossType t) {
    switch (t) {
        case BossType::SHADOW_KNIGHT: return "shadow_knight";
        case BossType::NECROMANCER:   return "necromancer";
        case BossType::VAMPIRE:       return "vampire";
        case BossType::FIRE_DEMON:    return "fire_demon";
        case BossType::GOLEM:         return "golem";
        case BossType::DEMON_LORD:    return "demon_lord";
        default: return "shadow_knight";
    }
}

// ============================================================
// G1 Step6: Boss 技能描述文字 (供 UI 展示)
// ============================================================
const char* get_boss_skills_text(const BossDef* def) {
    if (!def) return "冲锋·冲击波·召唤手下";
    if (def->is_summoner) return "召唤·尸爆·亡灵大军";
    if (def->is_defender) return "重锤·震地·石甲";
    return "冲锋·冲击波·召唤手下";
}

// ============================================================
// G1 Step6: spawn_boss — deprecated wrapper (委托给 BossFactory)
// ============================================================
Monster* spawn_boss(int tile_x, int tile_y, int floor) {
    BossType type = (floor == 5) ? BossType::SHADOW_KNIGHT
                   : (floor == 10) ? BossType::FIRE_DEMON
                   : BossType::DEMON_LORD;
    return boss_factory_create(type, tile_x, tile_y, floor);
}

// ============================================================
// D8 Step2: boss_type_for_floor — 随机Boss池
// ============================================================
BossType boss_type_for_floor(int floor, uint32_t seed) {
    auto rng_local = std::mt19937(seed ? seed : (uint32_t)std::random_device{}());
    if (floor == 5) {
        // G1 Step6: 3 种 Boss 随机 (Shadow Knight / Necromancer / Vampire)
        int roll = (int)(rng_local() % 3);
        if (roll == 0) return BossType::SHADOW_KNIGHT;
        if (roll == 1) return BossType::NECROMANCER;
        return BossType::VAMPIRE;
    } else if (floor == 10) {
        return (rng_local() % 2 == 0) ? BossType::FIRE_DEMON : BossType::GOLEM;
    }
    return BossType::DEMON_LORD; // F15 固定
}

// ============================================================
// G1 Step6: boss_factory_create — 数据驱动 Boss 创建
// BossDef (bosses.json) → BossAI 参数设置 → Monster
// ============================================================
Monster* boss_factory_create(BossType type, int tile_x, int tile_y, int floor,
                              std::vector<Monster*>* out_monsters, GameMap* map) {
    const BossDef* def = get_boss_def(_boss_type_to_id(type));
    if (!def) {
        // 安全降级: 配置缺失时使用默认 Shadow Knight
        def = get_boss_def("shadow_knight");
        if (!def) {
            auto* boss = new Monster((float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
                "暗影骑士", 250, 15, 10, 4, get_boss_visual_color("shadow_knight"), new BossAI());
            boss->is_boss = true;
            boss->entity.size = {48, 48};
            boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};
            return boss;
        }
    }

    BossAI* ai = new BossAI();
    Monster* boss = new Monster(
        (float)tile_x * TILE_SIZE, (float)tile_y * TILE_SIZE,
        def->name, def->hp, def->atk, def->pdef, def->mdef,
        get_boss_visual_color(def->visual_id), ai);
    boss->is_boss = true;
    boss->entity.size = {48, 48};
    boss->entity.rect = {boss->entity.position.x, boss->entity.position.y, 48, 48};

    // ── G1 Step6: Phase2 参数 (替代硬编码) ──
    ai->_phase2_hp_threshold = def->phase2_hp_threshold;
    ai->_phase2_pause        = def->phase2_pause;
    ai->_phase2_speed_mult   = def->phase2_speed_mult;
    ai->_phase2_atk_mult     = def->phase2_atk_mult;
    ai->_phase2_cd_mult      = def->phase2_cd_mult;

    // ── G5.4: Boss ID 用于 Phase2 行为分支 ──
    ai->_boss_id = def->id.c_str();

    // ── G2.3: Arena 配置 ──
    ai->_arena_cfg = &def->arena;

    // ── 技能循环 bias ──
    ai->skill_cycle_bias = def->skill_cycle_bias;

    // ── 技能冷却覆盖 (从 BossDef → BossSkill) ──
    for (auto& sk : def->skill_overrides) {
        if (sk.id == "charge" || sk.id == "冲锋")
            ai->_charge->cooldown = sk.cooldown;
        else if (sk.id == "shockwave" || sk.id == "冲击波")
            ai->_shockwave->cooldown = sk.cooldown;
        else if (sk.id == "summon" || sk.id == "召唤")
            ai->_summon->cooldown = sk.cooldown;
    }

    // ── 特殊行为标志 ──
    if (def->is_summoner) {
        ai->skill_cycle_bias = 4;  // Necromancer 召唤特化
    }
    if (def->is_defender) {
        ai->golem_shield_pct = def->shield_pct;
    }

    (void)out_monsters; (void)map;
    return boss;
}
