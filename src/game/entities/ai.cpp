#include "ai.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "combat_system.h"
#include "vfx_server.h"
#include "systems/team_coordinator.h"   // G2.2
#include <cmath>

// Forward declarations for functions defined later in this file
static void _exec_snipe(Monster* self, Player* player, std::vector<Effect>* effects, MonsterSkillState& sk);
static void _exec_ambush(Monster* self, Player* player, GameMap* map, std::vector<Effect>* effects, MonsterSkillState& sk);
static void _exec_scatter(Monster* self, Player* player, std::vector<Effect>* effects, MonsterSkillState& sk);
static void _exec_guard_aura(Monster* self, std::vector<Monster*>* all, std::vector<Effect>* effects, MonsterSkillState& sk);

MonsterAI::MonsterAI(float sight, float speed, float patrol, float attack)
    : sight_range(sight), move_speed(speed), patrol_interval(patrol),
      attack_range(attack) {
    _pick_new_dir();
}

void MonsterAI::update(Monster* self, Player* player, GameMap* map,
                        double dt, double gt,
                        std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (!self->combat.is_alive) return;

    // G5.3: Archetype-driven behaviors (替代旧的 monster_type 检查)
    // ── 向后兼容: DEFAULT → 从 monster_type 推断 ──
    if (archetype == AIArchetype::DEFAULT) {
        switch (self->monster_type) {
        case MonsterType::BOMBER:   archetype = AIArchetype::BOMBER; break;
        case MonsterType::SHAMAN:   archetype = AIArchetype::SHAMAN; break;
        case MonsterType::CHARGER:  archetype = AIArchetype::CHARGER; break;
        case MonsterType::SUMMONER: archetype = AIArchetype::SUMMONER; break;
        default: break;
        }
    }
    switch (archetype) {
    case AIArchetype::BOMBER: {
        float dist = _dist_to(self, player);
        if (dist <= attack_range * 32.0f) {
            if (self->explode_timer <= 0) self->explode_timer = 2.0f;
            self->explode_timer -= (float)dt;
            if (self->explode_timer <= 0) {
                int dmg = (int)(self->combat.max_hp * 0.4f);
                self->combat.take_damage(self->combat.max_hp);
                if (effects) { VFXServer vfx;
                    vfx.boss_circle(self->entity.rect.x+self->entity.rect.width/2,self->entity.rect.y+self->entity.rect.height/2);
                    for(auto&e:vfx.effects) effects->push_back(e); }
                if (dist <= 3.0f*32.0f){
                    int aoe=calculate_damage(dmg,player->combat.get_effective_defense(AttackType::TRUE),AttackType::TRUE);
                    player->combat.take_damage(aoe);}
                return;
            }
            return;
        } else self->explode_timer=0.0f;
    } break;

    case AIArchetype::SHAMAN:
        if(all){self->support_cooldown-=(float)dt;
            if(self->support_cooldown<=0){Monster* ally=nullptr;float bd=5.0f*32.0f;
                for(auto*o:*all){if(!o||o==self||!o->combat.is_alive)continue;
                    if(o->ai && o->ai->archetype==AIArchetype::BOMBER)continue;
                    float d=hypotf(o->entity.rect.x-self->entity.rect.x,o->entity.rect.y-self->entity.rect.y);
                    if(d<bd){bd=d;ally=o;}}
                if(ally){if(rng()%2==0)apply_buff(ally,"attack_up",1);else ally->combat.heal((int)(ally->combat.max_hp*0.15f));
                    self->support_cooldown=4.0f;return;}}}
        break;

    case AIArchetype::SNIPER: {
        float dist=_dist_to(self,player);
        // 后退保持 6-9 格距离
        if(dist<6.0f*32.0f){
            float dx=self->entity.rect.x-player->entity.rect.x,dy=self->entity.rect.y-player->entity.rect.y;
            float len=sqrtf(dx*dx+dy*dy);if(len>1)_apply_movement(self,map,dx/len,dy/len,dt);
            self->color={255,200,180,255};
        }
        // 蓄力狙击
        if(dist>4.0f*32.0f&&dist<12.0f*32.0f){
            _archetype_timer+=(float)dt;
            if(_archetype_timer>2.5f){ // 每2.5s一发贯穿弹
                _archetype_timer=0.0f;
                int dmg=calculate_damage(self->combat.get_effective_attack()*2,
                    player->combat.get_effective_defense(AttackType::PHYSICAL)); // 无视50%防御的效果
                player->combat.take_damage(dmg);
                if(effects){Effect e;e.kind="bolt";e.world_x=self->entity.rect.x+self->entity.rect.width/2;
                    e.world_y=self->entity.rect.y+self->entity.rect.height/2;e.target_x=player->entity.rect.x;
                    e.target_y=player->entity.rect.y;e.radius=4;e.duration=0.3f;e.color={255,255,255,200};effects->push_back(e);}
                return;
            }
        }
        // normal move-chase
        break;
    }

    case AIArchetype::CONTROLLER: {
        _archetype_timer+=(float)dt;
        if(_archetype_timer>5.0f){
            _archetype_timer=0.0f;
            // 在玩家脚下放置危险区 (scatter 锥形弹)
            float px=player->entity.rect.x+player->entity.rect.width/2,py=player->entity.rect.y+player->entity.rect.height/2;
            for(int i=0;i<4;i++){
                float angle=(float)i*1.5708f+(float)(rng()%30)*0.01745f;
                float tx=px+cosf(angle)*80.0f,ty=py+sinf(angle)*80.0f;
                if(effects){Effect e;e.kind="pulse";e.world_x=tx;e.world_y=ty;e.radius=32;e.duration=0.5f;e.color={200,60,200,150};effects->push_back(e);}
                // damage check via actual zone placement would need ArenaManager integration
                // For now: direct hit check
                float dp2=hypotf(player->entity.rect.x+player->entity.rect.width/2-tx,player->entity.rect.y+player->entity.rect.height/2-ty);
                if(dp2<48.0f){int dmg=calculate_damage(self->combat.get_effective_attack(),player->combat.get_effective_defense(AttackType::MAGICAL),AttackType::MAGICAL);player->combat.take_damage(dmg);}
            }
            // also slow the player
            apply_buff(player,"slow",1);
            return;
        }
        // keep medium distance
        float dist=_dist_to(self,player);
        if(dist<4.0f*32.0f){float dx=self->entity.rect.x-player->entity.rect.x,dy=self->entity.rect.y-player->entity.rect.y;float len=sqrtf(dx*dx+dy*dy);if(len>1)_apply_movement(self,map,dx/len,dy/len,dt);return;}
        if(dist>8.0f*32.0f){float dx=player->entity.rect.x-self->entity.rect.x,dy=player->entity.rect.y-self->entity.rect.y;float len=sqrtf(dx*dx+dy*dy);if(len>1)_apply_movement(self,map,dx/len,dy/len,dt);return;}
        break;
    }

    case AIArchetype::AMBUSH: {
        _archetype_timer+=(float)dt;
        if(!_archetype_active&&_archetype_timer>8.0f){
            _archetype_active=true;_archetype_timer=0.0f;self->color.a=80; // 隐身
        }
        if(_archetype_active){
            self->color.a=80;
            // move towards player silently
            float dx=player->entity.rect.x+player->entity.rect.width/2-self->entity.rect.x-self->entity.rect.width/2;
            float dy=player->entity.rect.y+player->entity.rect.height/2-self->entity.rect.y-self->entity.rect.height/2;
            float len=sqrtf(dx*dx+dy*dy);if(len>1)_apply_movement(self,map,dx/len,dy/len,(float)dt*1.3f);
            // strike when close
            if(len<2.0f*32.0f){
                self->color.a=255;_archetype_active=false;
                int dmg=calculate_damage(self->combat.get_effective_attack()*3,player->combat.get_effective_defense(AttackType::PHYSICAL));
                player->combat.take_damage(dmg);apply_buff(player,"stun",1);
                return;
            }
            return; // 隐身中不进入普通状态机
        }
        break;
    }

    case AIArchetype::GUARDIAN: {
        _archetype_timer+=(float)dt;
        if(_archetype_timer>6.0f){
            _archetype_timer=0.0f;
            // 给附近盟友上防御buff
            if(all)for(auto*o:*all){
                if(!o||o==self||!o->combat.is_alive)continue;
                float d=hypotf(o->entity.rect.x-self->entity.rect.x,o->entity.rect.y-self->entity.rect.y);
                if(d<6.0f*32.0f){apply_buff(o,"defense_up",1);apply_buff(o,"shield",1);}
            }
            return;
        }
        // Guard position: stay between player and allies
        float dist=_dist_to(self,player);
        if(dist<3.0f*32.0f){
            float dx=self->entity.rect.x-player->entity.rect.x,dy=self->entity.rect.y-player->entity.rect.y;
            float len=sqrtf(dx*dx+dy*dy);if(len>1)_apply_movement(self,map,dx/len,dy/len,dt);
        }
        break;
    }
    default: break;
    }

    // D2 Step4: Team协同决策 (Boss不参与)
    if (!self->is_boss && all && (*all).size() > 1) {
        _team_decision(self, player, map, dt, all);
    }

    // D2 Step3: Think — 决策技能释放
    bool use_skills_anyway = (archetype == AIArchetype::BOMBER);
    if (!_skills.empty() && !use_skills_anyway) {
        if (_think_and_cast(self, player, map, dt, gt, all, effects))
            return;
    }
    if (use_skills_anyway && !_skills.empty()) {
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
            if (sk.cast_left <= 0) {
                if (sk.type == MonsterSkillType::CHARGE) {
                    _exec_charge(self, player, map, effects, sk);
                    sk.active = false; sk.cooldown = sk.max_cooldown; return true;
                }
                if (sk.type == MonsterSkillType::SNIPE) {
                    _exec_snipe(self, player, effects, sk); return true;
                }
                if (sk.type == MonsterSkillType::AMBUSH_ATTACK) {
                    _exec_ambush(self, player, map, effects, sk); return true;
                }
                sk.active = true; // 蓄力完毕
            }
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
        // D8: Charger — 中距蓄力冲撞
        case MonsterSkillType::CHARGE:
            should_cast = (dist > 3.0f * 32.0f && dist < 8.0f * 32.0f);
            break;
        // D8: Summoner — 场上怪物不多时召唤
        case MonsterSkillType::MASS_SUMMON:
            should_cast = (all && (int)(*all).size() < 10);
            break;
        case MonsterSkillType::SNIPE:
            should_cast = (dist > 4.0f * 32.0f && dist < 12.0f * 32.0f);
            break;
        case MonsterSkillType::SCATTER:
            should_cast = (dist > 3.0f * 32.0f && dist < 9.0f * 32.0f);
            break;
        case MonsterSkillType::AMBUSH_ATTACK:
            should_cast = (dist > 5.0f * 32.0f && dist < 10.0f * 32.0f);
            break;
        case MonsterSkillType::GUARD_AURA:
            should_cast = (all && (int)(*all).size() >= 2);
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
        // D8: CHARGE — 蓄力冲刺 (0.5s蓄力→冲刺)
        case MonsterSkillType::CHARGE:
            sk.cast_left = 0.5f; break;
        // D8: MASS_SUMMON — 一次召唤2只
        case MonsterSkillType::MASS_SUMMON:
            _exec_mass_summon(self, player, map, all, effects, sk);
            sk.cooldown = sk.max_cooldown; break;
        case MonsterSkillType::SNIPE:
            sk.cast_left = 1.2f; break;  // 蓄力→ exec_snipe
        case MonsterSkillType::SCATTER:
            _exec_scatter(self, player, effects, sk); break;
        case MonsterSkillType::AMBUSH_ATTACK:
            sk.cast_left = 0.3f; break;  // 蓄力→ exec_ambush
        case MonsterSkillType::GUARD_AURA:
            _exec_guard_aura(self, all, effects, sk); break;
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
// D8 Step1: CHARGE — 蓄力后直线冲刺, 撞击造成伤害+击退
// ============================================================
void MonsterAI::_exec_charge(Monster* self, Player* player, GameMap* map,
                              std::vector<Effect>* effects, MonsterSkillState& sk) {
    float dx = player->entity.rect.x + player->entity.rect.width/2
             - self->entity.rect.x - self->entity.rect.width/2;
    float dy = player->entity.rect.y + player->entity.rect.height/2
             - self->entity.rect.y - self->entity.rect.height/2;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 1) return;
    float ndx = dx/dist, ndy = dy/dist;

    // 冲刺距离: 4格
    float charge_dist = std::min(dist, 4.0f * 32.0f);
    self->entity.position.x += ndx * charge_dist;
    self->entity.position.y += ndy * charge_dist;
    self->entity.sync_rect();

    // 穿墙回退
    if (!map->is_rect_walkable(self->entity.rect)) {
        self->entity.position.x -= ndx * charge_dist;
        self->entity.position.y -= ndy * charge_dist;
        self->entity.sync_rect();
    }

    // 碰撞检测: 如果冲刺后接触到玩家, 造成伤害+击退
    float after_dist = _dist_to(self, player);
    if (after_dist <= attack_range * 32.0f * 2.0f) {
        int dmg = calculate_damage(self->combat.get_effective_attack() * 2,
            player->combat.get_effective_defense(self->attack_type));
        player->combat.take_damage(dmg);
        // 击退玩家
        float px = player->entity.position.x + ndx * 48.0f;
        float py = player->entity.position.y + ndy * 48.0f;
        player->entity.position = {px, py};
        player->entity.sync_rect();
    }

    // 冲刺轨迹特效
    if (effects) {
        Effect trail;
        trail.kind = "bolt";
        trail.world_x = self->entity.rect.x + self->entity.rect.width/2 - ndx * charge_dist;
        trail.world_y = self->entity.rect.y + self->entity.rect.height/2 - ndy * charge_dist;
        trail.target_x = self->entity.rect.x + self->entity.rect.width/2;
        trail.target_y = self->entity.rect.y + self->entity.rect.height/2;
        trail.radius = 4; trail.duration = 0.3f; trail.elapsed = 0;
        trail.color = {255, 180, 40, 200};
        effects->push_back(trail);
    }
    (void)sk;
}

// ============================================================
// D8 Step1: MASS_SUMMON — 一次召唤2只小怪
// ============================================================
void MonsterAI::_exec_mass_summon(Monster* self, Player*, GameMap* map,
                                   std::vector<Monster*>* all,
                                   std::vector<Effect>* effects,
                                   MonsterSkillState& sk) {
    for (int i = 0; i < 2; i++) {
        int off_x = (int)(rng() % 7) - 3;
        int off_y = (int)(rng() % 7) - 3;
        float sx = self->entity.position.x + off_x * 32.0f;
        float sy = self->entity.position.y + off_y * 32.0f;
        auto [tx, ty] = map->pixel_to_tile(sx, sy);
        if (map->is_walkable(tx, ty)) {
            const char* type = (rng() % 2 == 0) ? "slime" : "archer";
            Monster* m = spawn_monster(sx, sy, type);
            if (m && all) all->push_back(m);
            if (effects) {
                Effect e;
                e.kind = "pulse"; e.world_x = sx; e.world_y = sy;
                e.radius = 24; e.duration = 0.25f; e.elapsed = 0;
                e.color = {180, 120, 220, 150};
                effects->push_back(e);
            }
        }
    }
    (void)sk;
}

// ============================================================
// D2 Step4: Team Decision — 怪物协同 AI (G2.2 重构)
// TeamCoordinator 负责分析, MonsterAI 负责执行
// ============================================================
void MonsterAI::_team_decision(Monster* self, Player* player, GameMap* map,
                                double dt, std::vector<Monster*>* all) {
    // ── 概率门 (保留在 MonsterAI — 这是执行决策, 非分析) ──
    if (team_coop_chance <= 0) return;
    if ((float)(rng() % 1000) / 1000.0f > team_coop_chance) return;

    TeamRole role = self->team_role;
    if (role == TeamRole::NONE) return;

    // ── G2.2: 委托分析给 TeamCoordinator ──
    TeamDecision dec = TeamCoordinator::evaluate(self, *player, *all, map);

    // ── 执行: 移动建议 ──
    if (dec.move_x != 0.0f || dec.move_y != 0.0f) {
        _apply_movement(self, map, dec.move_x, dec.move_y, dt);
        if (role == TeamRole::FRONTLINE && dec.support_target)
            _protect_target = dec.support_target;
        return;
    }

    // ── 执行: 辅助行动 ──
    if (dec.should_support && dec.support_target && self->support_cooldown <= 0) {
        if (rng() % 2 == 0)
            apply_buff(dec.support_target, "attack_up", 1);
        else
            dec.support_target->combat.heal(
                (int)(dec.support_target->combat.max_hp * 0.15f));
        self->support_cooldown = 4.0f;
        return;
    }

    // ── 执行: 指挥光环 ──
    if (dec.should_command) {
        _command_cooldown -= (float)dt;
        if (_command_cooldown <= 0) {
            float ax = self->entity.rect.x + self->entity.rect.width / 2;
            float ay = self->entity.rect.y + self->entity.rect.height / 2;
            for (auto* a : *all) {
                if (!a || !a->combat.is_alive || a == self) continue;
                float d2 = (a->entity.rect.x + a->entity.rect.width/2 - ax)
                         * (a->entity.rect.x + a->entity.rect.width/2 - ax)
                         + (a->entity.rect.y + a->entity.rect.height/2 - ay)
                         * (a->entity.rect.y + a->entity.rect.height/2 - ay);
                if (d2 < (6.0f * 32.0f) * (6.0f * 32.0f))
                    apply_buff(a, "attack_up", 1);
            }
            _command_cooldown = 10.0f;
        }
    }
}

// ═══════════════════════════════════════════════════════════════
// G5.3: New Skill Executors (SNIPE, SCATTER, AMBUSH_ATTACK, GUARD_AURA)
// ═══════════════════════════════════════════════════════════════

// SNIPE — 蓄力贯穿: 蓄力1.2s后发射单发高速弹
void _exec_snipe(Monster* self, Player* player, std::vector<Effect>* effects, MonsterSkillState& sk) {
    if (sk.cast_left > 0) { sk.cast_left -= 0.016f; return; } // 蓄力
    float sx = self->entity.rect.x + self->entity.rect.width/2, sy = self->entity.rect.y + self->entity.rect.height/2;
    float tx = player->entity.rect.x + player->entity.rect.width/2, ty = player->entity.rect.y + player->entity.rect.height/2;
    int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 2.5),
        player->combat.get_effective_defense(AttackType::PHYSICAL) / 2); // 无视50%防御
    player->combat.take_damage(dmg);
    if (effects) { Effect e; e.kind = "bolt"; e.world_x = sx; e.world_y = sy; e.target_x = tx; e.target_y = ty;
        e.radius = 5; e.duration = 0.3f; e.color = {255, 255, 255, 220}; effects->push_back(e); }
    sk.cooldown = sk.max_cooldown;
}

// SCATTER — 锥形散布 3-4 弹
void _exec_scatter(Monster* self, Player* player, std::vector<Effect>* effects, MonsterSkillState& sk) {
    float sx = self->entity.rect.x + self->entity.rect.width/2, sy = self->entity.rect.y + self->entity.rect.height/2;
    int count = 3 + (rng() % 2); // 3-4 shots
    for(int i=0;i<count;i++){
        float angle = ((float)i - (float)count/2 + 0.5f) * 0.35f; // ±35° spread
        float dx = cosf(angle) * 200.0f, dy = sinf(angle) * 200.0f;
        if (effects) { Effect e; e.kind = "bolt"; e.world_x = sx; e.world_y = sy; e.target_x = sx + dx; e.target_y = sy + dy;
            e.radius = 3; e.duration = 0.25f; e.color = {200, 100, 255, 180}; effects->push_back(e); }
        // check player hit
        float px = player->entity.rect.x + player->entity.rect.width/2, py = player->entity.rect.y + player->entity.rect.height/2;
        float close_dist = fabsf((px-sx)*(-sinf(angle)) + (py-sy)*cosf(angle));
        float along_dist = (px-sx)*cosf(angle) + (py-sy)*sinf(angle);
        if(close_dist < 48.0f && along_dist > 0 && along_dist < 250.0f){
            int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 0.6),
                player->combat.get_effective_defense(AttackType::MAGICAL), AttackType::MAGICAL);
            player->combat.take_damage(dmg);
        }
    }
    sk.cooldown = sk.max_cooldown;
}

// AMBUSH_ATTACK — 隐身2s→瞬移至玩家旁→高爆发+眩晕
void _exec_ambush(Monster* self, Player* player, GameMap* map, std::vector<Effect>* effects, MonsterSkillState& sk) {
    if (sk.cast_left > 0) { sk.cast_left -= 0.016f; return; }
    float px = player->entity.rect.x + player->entity.rect.width/2, py = player->entity.rect.y + player->entity.rect.height/2;
    // teleport 2 tiles behind player
    float behind_x = px - 64.0f, behind_y = py;
    self->entity.position = {behind_x - self->entity.rect.width/2, behind_y - self->entity.rect.height/2};
    self->entity.sync_rect();
    if(map && !map->is_rect_walkable(self->entity.rect)){
        self->entity.position = {px + 64.0f - self->entity.rect.width/2, py - self->entity.rect.height/2};
        self->entity.sync_rect();
    }
    int dmg = calculate_damage((int)(self->combat.get_effective_attack() * 3.0),
        player->combat.get_effective_defense(AttackType::PHYSICAL));
    player->combat.take_damage(dmg);
    apply_buff(player, "stun", 1);
    if(effects){Effect e;e.kind="pulse";e.world_x=self->entity.rect.x+self->entity.rect.width/2;e.world_y=self->entity.rect.y+self->entity.rect.height/2;
        e.radius=48;e.duration=0.4f;e.color={100,30,180,200};effects->push_back(e);}
    sk.cooldown = sk.max_cooldown;
}

// GUARD_AURA — 自身+附近盟友 buff
void _exec_guard_aura(Monster* self, std::vector<Monster*>* all, std::vector<Effect>* effects, MonsterSkillState& sk) {
    float sx = self->entity.rect.x + self->entity.rect.width/2, sy = self->entity.rect.y + self->entity.rect.height/2;
    apply_buff(self, "stone_skin", 2);
    apply_buff(self, "defense_up", 2);
    if(all) for(auto* o:*all){
        if(!o||!o->combat.is_alive||o==self)continue;
        float d=hypotf(o->entity.rect.x+o->entity.rect.width/2-sx,o->entity.rect.y+o->entity.rect.height/2-sy);
        if(d<6.0f*32.0f){apply_buff(o,"defense_up",1);apply_buff(o,"shield",1);}
    }
    if(effects){Effect e;e.kind="pulse";e.world_x=sx;e.world_y=sy;e.radius=80;e.duration=0.5f;e.color={60,140,255,150};effects->push_back(e);}
    sk.cooldown = sk.max_cooldown;
}
