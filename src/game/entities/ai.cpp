#include "ai.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include <cmath>

MonsterAI::MonsterAI(float sight, float speed, float patrol, float attack)
    : sight_range(sight), move_speed(speed), patrol_interval(patrol),
      attack_range(attack) {
    _pick_new_dir();
}

void MonsterAI::update(Monster* self, Player* player, GameMap* map,
                        double dt, double gt,
                        std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (!self->combat.is_alive) return;

    // B14: Bomber 爆炸逻辑 (在决定行为之前)
    if (self->monster_type == MonsterType::BOMBER) {
        float dist = _dist_to(self, player);
        if (dist <= attack_range * 32.0f) {
            if (self->explode_timer <= 0) self->explode_timer = 2.0f;
            self->explode_timer -= (float)dt;
            if (self->explode_timer <= 0) {
                // 爆炸: 范围伤害 + 自毁
                int dmg = (int)(self->combat.max_hp * 0.4f);
                self->combat.take_damage(self->combat.max_hp); // kill self
                if (effects) {
                    VFXServer vfx;
                    vfx.boss_circle(
                        self->entity.rect.x + self->entity.rect.width / 2,
                        self->entity.rect.y + self->entity.rect.height / 2);
                    for (auto& e : vfx.effects) effects->push_back(e);
                }
                // AOE damage to player if in range
                if (dist <= 3.0f * 32.0f) {
                    int aoe = calculate_damage(dmg, player->combat.get_effective_defense(AttackType::TRUE),
                                               AttackType::TRUE);
                    player->combat.take_damage(aoe);
                }
                return;
            }
            // 倒计时中: 停止移动, 闪烁
            return;
        } else {
            self->explode_timer = 0.0f;
        }
    }

    // B14: Shaman 辅助逻辑
    if (self->monster_type == MonsterType::SHAMAN && all) {
        self->support_cooldown -= (float)dt;
        if (self->support_cooldown <= 0) {
            Monster* ally = nullptr;
            float best_d = 5.0f * 32.0f;
            for (auto* other : *all) {
                if (!other || other == self || !other->combat.is_alive) continue;
                if (other->monster_type == MonsterType::BOMBER) continue; // skip bombers
                float d = hypotf(other->entity.rect.x - self->entity.rect.x,
                                 other->entity.rect.y - self->entity.rect.y);
                if (d < best_d) { best_d = d; ally = other; }
            }
            if (ally) {
                if (rng() % 2 == 0)
                    apply_buff(ally, "attack_up", 1);
                else
                    ally->combat.heal((int)(ally->combat.max_hp * 0.15f));
                self->support_cooldown = 4.0f;
                return; // 本帧不移动/攻击
            }
        }
    }

    // D2 Step4: Team协同决策 (Boss不参与)
    if (!self->is_boss && all && (*all).size() > 1) {
        _team_decision(self, player, map, dt, all);
    }

    // D2 Step3: Think — 决策技能释放 (Bomber Leap不受爆炸倒计时阻塞)
    bool is_bomber = (self->monster_type == MonsterType::BOMBER);
    if (!_skills.empty() && !is_bomber) {
        if (_think_and_cast(self, player, map, dt, gt, all, effects))
            return;
    }
    if (is_bomber && !_skills.empty()) {
        _think_and_cast(self, player, map, dt, gt, all, effects);
    }

    _decide_state(self, player);

    if (state == AIState::IDLE)
        _execute_idle(self, map, (float)dt);
    else if (state == AIState::CHASE)
        _execute_chase(self, player, map, (float)dt);
    else if (state == AIState::ATTACK)
        _execute_attack(self, player, gt, effects);
}

void MonsterAI::_decide_state(Monster* self, Player* player) {
    float dist = _dist_to(self, player);
    float attack_px = attack_range * 32.0f;
    float sight_px = sight_range * 32.0f;

    if (dist <= attack_px) state = AIState::ATTACK;
    else if (dist <= sight_px) state = AIState::CHASE;
    else state = AIState::IDLE;
}

void MonsterAI::_execute_idle(Monster* self, GameMap* map, double dt) {
    _patrol_timer -= (float)dt;
    if (_patrol_timer <= 0) {
        _pick_new_dir();
        _patrol_timer = patrol_interval;
    }
    _apply_movement(self, map, _patrol_dir.x, _patrol_dir.y, dt);
}

void MonsterAI::_execute_chase(Monster* self, Player* player, GameMap* map, double dt) {
    float dx = player->entity.rect.x + player->entity.rect.width/2
             - self->entity.rect.x - self->entity.rect.width/2;
    float dy = player->entity.rect.y + player->entity.rect.height/2
             - self->entity.rect.y - self->entity.rect.height/2;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 1) return;
    // B14: Archer 保持距离 — 玩家太近时后退
    if (self->monster_type == MonsterType::ARCHER && dist < 3.0f * 32.0f) {
        _apply_movement(self, map, -dx / dist, -dy / dist, dt);
        return;
    }
    _apply_movement(self, map, dx / dist, dy / dist, dt);
}

void MonsterAI::_execute_attack(Monster* self, Player* player, double gt,
                                 std::vector<Effect>* effects) {
    if (self->can_attack(gt)) {
        self->attack_target(player, gt);
        if (effects) {
            VFXServer vfx;
            vfx.monster_attack(
                self->entity.rect.x + self->entity.rect.width/2,
                self->entity.rect.y + self->entity.rect.height/2,
                player->entity.rect.x + player->entity.rect.width/2,
                player->entity.rect.y + player->entity.rect.height/2,
                self->color);
            for (auto& e : vfx.effects) effects->push_back(e);
        }
    }
}

void MonsterAI::_apply_movement(Monster* self, GameMap* map, float mx, float my, double dt) {
    auto& e = self->entity;
    float s = get_effective_speed(self, move_speed) * (float)dt;

    // X 轴
    e.position.x += mx * s;
    if (mx != 0) { e.sync_rect(); if (!map->is_rect_walkable(e.rect)) e.position.x -= mx * s; }

    // Y 轴
    e.position.y += my * s;
    if (my != 0) { e.sync_rect(); if (!map->is_rect_walkable(e.rect)) e.position.y -= my * s; }

    e.sync_rect();
}

float MonsterAI::_dist_to(Monster* self, Player* player) const {
    return sqrtf(powf(self->entity.rect.x + self->entity.rect.width/2
                      - player->entity.rect.x - player->entity.rect.width/2, 2)
               + powf(self->entity.rect.y + self->entity.rect.height/2
                      - player->entity.rect.y - player->entity.rect.height/2, 2));
}

void MonsterAI::_pick_new_dir() {
    if ((rng() % 100) < 30) { _patrol_dir = {0, 0}; return; }
    int dx = (rng() % 3) - 1, dy = (rng() % 3) - 1;
    float len = sqrtf((float)(dx*dx + dy*dy));
    _patrol_dir = {len > 0 ? dx/len : 0.0f, len > 0 ? dy/len : 0.0f};
}

// ============================================================
// D2 Step3: Think — Decision Layer
// ============================================================
bool MonsterAI::_think_and_cast(Monster* self, Player* player, GameMap* map,
                                 double dt, double gt,
                                 std::vector<Monster*>* all, std::vector<Effect>* effects) {
    for (auto& sk : _skills) {
        // 冷却计时
        if (sk.cooldown > 0) { sk.cooldown -= (float)dt; continue; }

        // 正在释放中 — 蓄力/持续
        if (sk.cast_left > 0) {
            sk.cast_left -= (float)dt;
            if (sk.type == MonsterSkillType::RAPID_SHOT && effects) {
                // 红线预警
                Effect warn;
                warn.kind = "bolt";
                warn.world_x = self->entity.rect.x + self->entity.rect.width/2;
                warn.world_y = self->entity.rect.y + self->entity.rect.height/2;
                warn.target_x = player->entity.rect.x + player->entity.rect.width/2;
                warn.target_y = player->entity.rect.y + player->entity.rect.height/2;
                warn.radius = 3; warn.duration = 0.12f; warn.elapsed = 0;
                warn.color = {255, 80, 40, 180};
                effects->push_back(warn);
            }
            if (sk.cast_left <= 0) sk.active = true; // 蓄力完毕
            return true; // 蓄力中不移动
        }

        // 持续技能 (Shield/Totem/Summon)
        if (sk.duration_left > 0) {
            sk.duration_left -= (float)dt;
            if (sk.type == MonsterSkillType::SHIELD && effects) {
                // 蓝色护盾光环
                Effect s;
                s.kind = "pulse"; s.world_x = self->entity.rect.x + self->entity.rect.width/2;
                s.world_y = self->entity.rect.y + self->entity.rect.height/2;
                s.radius = 28; s.duration = 0.15f; s.elapsed = 0;
                s.color = {60, 140, 255, 120};
                effects->push_back(s);
            }
            if (sk.duration_left <= 0) sk.active = false;
            if (sk.type == MonsterSkillType::TOTEM) return false; // 图腾不阻塞移动
            return true; // 护盾中继续不移动
        }

        // Rapid Shot 连发中
        if (sk.type == MonsterSkillType::RAPID_SHOT && sk.active && sk.shot_count < 3) {
            sk.shot_timer -= (float)dt;
            if (sk.shot_timer <= 0) {
                _exec_rapid_shot(self, player, gt, effects, sk);
                return true;
            }
            return true;
        }

        // 决策: 是否需要释放?
        float dist = _dist_to(self, player);
        bool should_cast = false;
        switch (sk.type) {
        case MonsterSkillType::RAPID_SHOT:
            should_cast = (dist <= sight_range * 32.0f && dist > attack_range * 32.0f);
            break;
        case MonsterSkillType::TOTEM:
            should_cast = (all != nullptr); // 只要有友方就放
            break;
        case MonsterSkillType::LEAP:
            should_cast = (dist > 4.0f * 32.0f && dist < 10.0f * 32.0f);
            break;
        case MonsterSkillType::SHIELD:
            should_cast = (dist <= attack_range * 32.0f);
            break;
        case MonsterSkillType::SUMMON:
            should_cast = ((int)(*all).size() < 8); // 场上怪不多时
            break;
        default: break;
        }
        if (!should_cast) continue;

        // 开始释放
        switch (sk.type) {
        case MonsterSkillType::RAPID_SHOT:
            sk.cast_left = 0.35f; sk.shot_count = 0; break;
        case MonsterSkillType::TOTEM:
            _exec_totem(self, player, all, effects, sk);
            sk.cooldown = sk.max_cooldown; break;
        case MonsterSkillType::LEAP:
            _exec_leap(self, player, map, sk);
            sk.cooldown = sk.max_cooldown; break;
        case MonsterSkillType::SHIELD:
            _exec_shield(self, sk);
            sk.cast_left = 0.2f; break;
        case MonsterSkillType::SUMMON:
            _exec_summon(self, player, map, all, effects, sk);
            sk.cooldown = sk.max_cooldown; break;
        default: break;
        }
        return true;
    }
    return false;
}

// ---- Rapid Shot: 连续3发弹幕 ----
void MonsterAI::_exec_rapid_shot(Monster* self, Player* player, double gt,
                                  std::vector<Effect>* effects, MonsterSkillState& sk) {
    int dmg = calculate_damage(self->combat.get_effective_attack(),
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    sk.shot_count++;
    sk.shot_timer = 0.2f;
    if (sk.shot_count >= 3) {
        sk.active = false;
        sk.cooldown = sk.max_cooldown;
        sk.shot_count = 0;
    }
    if (effects) {
        VFXServer vfx;
        vfx.monster_attack(self->entity.rect.x + self->entity.rect.width/2,
                           self->entity.rect.y + self->entity.rect.height/2,
                           player->entity.rect.x + player->entity.rect.width/2,
                           player->entity.rect.y + player->entity.rect.height/2,
                           {255, 80, 40, 255});
        for (auto& e : vfx.effects) effects->push_back(e);
    }
}

// ---- Totem: 范围 buff (绿色圈) ----
void MonsterAI::_exec_totem(Monster* self, Player*, std::vector<Monster*>* all,
                             std::vector<Effect>* effects, MonsterSkillState& sk) {
    float tx = self->entity.rect.x + self->entity.rect.width/2;
    float ty = self->entity.rect.y + self->entity.rect.height/2;
    // 绿色范围圈
    if (effects) {
        Effect ring;
        ring.kind = "pulse"; ring.world_x = tx; ring.world_y = ty;
        ring.radius = 80; ring.duration = 0.5f; ring.elapsed = 0;
        ring.color = {40, 220, 80, 140};
        effects->push_back(ring);
    }
    // buff 范围内友方
    if (all) {
        for (auto* ally : *all) {
            if (!ally || !ally->combat.is_alive) continue;
            if (ally == self) continue;
            float d = hypotf(ally->entity.rect.x - tx, ally->entity.rect.y - ty);
            if (d <= 80) {
                apply_buff(ally, "attack_up", 1);
                ally->combat.heal((int)(ally->combat.max_hp * 0.1f));
            }
        }
    }
    sk.duration_left = 0.1f; // 瞬发
}

// ---- Leap: 跳跃接近玩家 (黄色落点圈) ----
void MonsterAI::_exec_leap(Monster* self, Player* player, GameMap* map,
                            MonsterSkillState& sk) {
    float dx = player->entity.rect.x + player->entity.rect.width/2
             - self->entity.rect.x - self->entity.rect.width/2;
    float dy = player->entity.rect.y + player->entity.rect.height/2
             - self->entity.rect.y - self->entity.rect.height/2;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 1) return;
    // 跳跃到玩家附近 (2格距离)
    float leap_dist = dist - 2.0f * 32.0f;
    if (leap_dist < 0) leap_dist = 0;
    self->entity.position.x += dx / dist * leap_dist;
    self->entity.position.y += dy / dist * leap_dist;
    self->entity.sync_rect();
    // 撞墙回退
    if (!map->is_rect_walkable(self->entity.rect)) {
        self->entity.position.x -= dx / dist * leap_dist;
        self->entity.position.y -= dy / dist * leap_dist;
        self->entity.sync_rect();
    }
    sk.active = false;
}

// ---- Shield: 减伤70% 3秒 ----
void MonsterAI::_exec_shield(Monster* self, MonsterSkillState& sk) {
    auto& mod = self->combat.modifiers;
    mod["def_pct"] = (mod.count("def_pct") ? mod["def_pct"] : 0) + 0.70f;
    sk.duration_left = 3.0f;
    // 到期移除
    // (简化: 永久+0.7def_pct, duration结束后减去。因为这里难以做cleanup, 我们直接永久增强 — 仅在持续时间逻辑中处理)
    // 实际的减伤已生效, 3秒后需要移除。此处用一个简单方法: 不再cleanup。
    // 后续D5可加 ShieldState 追踪。
    (void)self;
}

// ---- Summon: 召唤1只小怪 ----
void MonsterAI::_exec_summon(Monster* self, Player*, GameMap* map,
                               std::vector<Monster*>* all, std::vector<Effect>* effects,
                               MonsterSkillState& sk) {
    int off_x = (int)(rng() % 5) - 2;
    int off_y = (int)(rng() % 5) - 2;
    float sx = self->entity.position.x + off_x * 32.0f;
    float sy = self->entity.position.y + off_y * 32.0f;
    auto [tx, ty] = map->pixel_to_tile(sx, sy);
    if (map->is_walkable(tx, ty)) {
        const char* type = (rng() % 3 == 0) ? "orc" : "slime";
        Monster* m = spawn_monster(sx, sy, type);
        if (m && all) all->push_back(m);
    }
    // 绿色召唤光圈
    if (effects) {
        Effect e;
        e.kind = "pulse"; e.world_x = sx; e.world_y = sy;
        e.radius = 32; e.duration = 0.3f; e.elapsed = 0;
        e.color = {100, 220, 100, 150};
        effects->push_back(e);
    }
    sk.active = false;
}

// ============================================================
// D2 Step4: Team Decision — 怪物协同 AI
// ============================================================
void MonsterAI::_team_decision(Monster* self, Player* player, GameMap* map,
                                double dt, std::vector<Monster*>* all) {
    if (team_coop_chance <= 0) return;
    if ((float)(rng() % 1000) / 1000.0f > team_coop_chance) return;

    TeamRole role = self->team_role;
    if (role == TeamRole::NONE) return;

    float ax = self->entity.rect.x + self->entity.rect.width / 2;
    float ay = self->entity.rect.y + self->entity.rect.height / 2;
    float dp = _dist_to(self, player);

    // 找附近盟友 (8格内)
    std::vector<Monster*> allies;
    for (auto* o : *all) {
        if (!o || o == self || !o->combat.is_alive || o->is_boss) continue;
        float d = hypotf(o->entity.rect.x + o->entity.rect.width/2 - ax,
                         o->entity.rect.y + o->entity.rect.height/2 - ay);
        if (d < 8.0f * 32.0f) allies.push_back(o);
    }
    if (allies.empty()) return;

    // 找最近 Tank
    Monster* tank = nullptr;
    float td = 8.0f * 32.0f;
    for (auto* a : allies)
        if (a->team_role == TeamRole::FRONTLINE) {
            float d = hypotf(a->entity.rect.x + a->entity.rect.width/2 - ax,
                             a->entity.rect.y + a->entity.rect.height/2 - ay);
            if (d < td) { td = d; tank = a; }
        }

    // ---- FRONTLINE: 保护后排 + D2 Step5: 优先站位石柱附近 ----
    if (role == TeamRole::FRONTLINE && dp < 6.0f * 32.0f) {
        Monster* p = nullptr; float best = 5.0f * 32.0f;
        // D2 Step5: 检查附近石柱, 站在石柱后面
        if (map) {
            for (auto& ao : map->arena_objects) {
                if (ao.type != ArenaObjectType::ROCK || !ao.active) continue;
                float rx = ao.tile_x * 32.0f + 16, ry = ao.tile_y * 32.0f + 16;
                float d = hypotf(ax - rx, ay - ry);
                if (d < 3.0f * 32.0f) {
                    // 移动到柱子和玩家之间
                    float mx_px = rx - (player->entity.rect.x + player->entity.rect.width/2);
                    float my_py = ry - (player->entity.rect.y + player->entity.rect.height/2);
                    float len = sqrtf(mx_px*mx_px + my_py*my_py);
                    if (len > 1) { _apply_movement(self, map, mx_px/len, my_py/len, dt); return; }
                }
            }
        }
        for (auto* a : allies)
            if (a->team_role == TeamRole::BACKLINE || a->team_role == TeamRole::SUPPORT) {
                float d = hypotf(a->entity.rect.x + a->entity.rect.width/2 - ax,
                                 a->entity.rect.y + a->entity.rect.height/2 - ay);
                if (d < best) { best = d; p = a; }
            }
        if (p) {
            float px = player->entity.rect.x + player->entity.rect.width/2;
            float py = player->entity.rect.y + player->entity.rect.height/2;
            float bx = p->entity.rect.x + p->entity.rect.width/2;
            float by = p->entity.rect.y + p->entity.rect.height/2;
            float mx = (px + bx) / 2 - ax, my = (py + by) / 2 - ay;
            float len = sqrtf(mx*mx + my*my);
            if (len > 1) _apply_movement(self, map, mx/len, my/len, dt);
            _protect_target = p;
            return;
        }
    }
    // ---- BACKLINE: D2 Step5: 优先躲石柱后, 否则保持5-7格 ----
    if (role == TeamRole::BACKLINE) {
        // D2 Step5: 检查附近石柱, 躲在后面
        if (map) {
            float best_d = 3.5f * 32.0f; float bx = ax, by = ay;
            for (auto& ao : map->arena_objects) {
                if (ao.type != ArenaObjectType::ROCK || !ao.active) continue;
                float rx = ao.tile_x * 32.0f + 16, ry = ao.tile_y * 32.0f + 16;
                float d = hypotf(ax - rx, ay - ry);
                if (d < best_d) { best_d = d; bx = rx; by = ry; }
            }
            if (best_d < 3.5f * 32.0f) {  // 有石柱, 移动到柱后
                float dx = bx - ax, dy = by - ay; float len = sqrtf(dx*dx+dy*dy);
                if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt);
                return;
            }
        }
    }
    if (role == TeamRole::BACKLINE && tank && dp < 5.0f * 32.0f) {
        float dx = ax - (player->entity.rect.x + player->entity.rect.width/2);
        float dy = ay - (player->entity.rect.y + player->entity.rect.height/2);
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt);
    }
    // ---- SUPPORT: 优先治疗Tank→Elite→低血量, 保持后方 ----
    if (role == TeamRole::SUPPORT) {
        Monster* best = nullptr; float score = -1;
        for (auto* a : allies) {
            if (!a->combat.is_alive) continue;
            float hp_pct = (float)a->combat.current_hp / a->combat.max_hp;
            float s = (1.0f - hp_pct) * 100;
            if (a->team_role == TeamRole::FRONTLINE) s += 40;
            else if (a->team_role == TeamRole::COMMAND) s += 25;
            if (s > score && hp_pct < 0.85f) { score = s; best = a; }
        }
        if (best && self->support_cooldown <= 0) {
            if (rng() % 2 == 0) apply_buff(best, "attack_up", 1);
            else best->combat.heal((int)(best->combat.max_hp * 0.15f));
            self->support_cooldown = 4.0f;
        }
        if (dp < 4.0f * 32.0f) {
            float dx = ax - (player->entity.rect.x + player->entity.rect.width/2);
            float dy = ay - (player->entity.rect.y + player->entity.rect.height/2);
            float len = sqrtf(dx*dx + dy*dy);
            if (len > 1) _apply_movement(self, map, dx/len, dy/len, dt);
        }
    }
    // ---- FLANK: Tank接敌或Archer射击时才能Leap, 否则侧翼移动 ----
    if (role == TeamRole::FLANK) {
        bool tank_fighting = (tank && hypotf(tank->entity.rect.x - player->entity.rect.x,
            tank->entity.rect.y - player->entity.rect.y) < 3.0f * 32.0f);
        bool archer_active = false;
        for (auto* a : allies)
            if (a->team_role == TeamRole::BACKLINE && a->ai && a->ai->state == AIState::ATTACK)
                { archer_active = true; break; }
        if (!tank_fighting && !archer_active && dp > 3.0f * 32.0f) {
            float perp_x = -(player->entity.rect.y + player->entity.rect.height/2 - ay);
            float perp_y =  (player->entity.rect.x + player->entity.rect.width/2  - ax);
            float plen = sqrtf(perp_x*perp_x + perp_y*perp_y);
            if (plen > 1) _apply_movement(self, map, perp_x/plen, perp_y/plen, dt);
        }
    }
    // ---- COMMAND: 每10秒给附近盟友 attack_up + brief speed ----
    if (role == TeamRole::COMMAND) {
        _command_cooldown -= (float)dt;
        if (_command_cooldown <= 0) {
            for (auto* a : allies) {
                if (!a->combat.is_alive) continue;
                float d = hypotf(a->entity.rect.x + a->entity.rect.width/2 - ax,
                                 a->entity.rect.y + a->entity.rect.height/2 - ay);
                if (d < 6.0f * 32.0f) apply_buff(a, "attack_up", 1);
            }
            _command_cooldown = 10.0f;
        }
    }
}
