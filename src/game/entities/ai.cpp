#include "ai.h"
#include "monster.h"
#include "player.h"
#include "game_map.h"
#include "vfx_server.h"

MonsterAI::MonsterAI(float sight, float speed, float patrol, float attack)
    : sight_range(sight), move_speed(speed), patrol_interval(patrol),
      attack_range(attack) {
    _pick_new_dir();
}

void MonsterAI::update(Monster* self, Player* player, GameMap* map,
                        double dt, double gt,
                        std::vector<Monster*>* all, std::vector<Effect>* effects) {
    if (!self->combat.is_alive) return;
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
    float s = move_speed * (float)dt;

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
