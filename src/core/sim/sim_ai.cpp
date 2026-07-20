#include "sim_ai.h"
#include "player.h"
#include "monster.h"
#include "game_map.h"
#include "combat_system.h"  // rng
#include <cmath>

void SimAI::start() { _frame = 0; _dir_timer = 0; _current_dir = -1; }
void SimAI::tick() { _frame++; }

Monster* SimAI::_find_nearest(const Player* player, const std::vector<Monster*>& monsters) const {
    Monster* best = nullptr; float bd = 99999;
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    for (auto* m : monsters) {
        if (!m || !m->combat.is_alive) continue;
        float d = hypotf(m->entity.rect.x + m->entity.rect.width/2 - px,
                         m->entity.rect.y + m->entity.rect.height/2 - py);
        if (d < bd) { bd = d; best = m; }
    }
    return best;
}

void SimAI::_pick_direction(const Player* player, const std::vector<Monster*>& monsters) {
    Monster* t = _find_nearest(player, monsters);
    float px = player->entity.rect.x + player->entity.rect.width/2;
    float py = player->entity.rect.y + player->entity.rect.height/2;
    if (t) {
        float dx = t->entity.rect.x + t->entity.rect.width/2 - px;
        float dy = t->entity.rect.y + t->entity.rect.height/2 - py;
        if (fabsf(dx) > fabsf(dy))
            _current_dir = (dx > 0) ? 3 : 2; // right / left
        else
            _current_dir = (dy > 0) ? 1 : 0; // down / up
    } else {
        _current_dir = rng() % 4; // random walk
    }
}

bool SimAI::is_action_just_pressed(const char* action_name,
    const Player* player, const std::vector<Monster*>& monsters,
    const GameMap* map, bool stairs_active, bool boss_intro_active) {
    if (!player) return false;

    // ── Boss intro → always confirm ──
    if (boss_intro_active && strcmp(action_name, "confirm") == 0)
        return _frame % 60 == 0; // wait ~1s then confirm

    // ── Stairs → always descend ──
    if (stairs_active && strcmp(action_name, "descend") == 0)
        return _frame % 30 == 0;

    // ── Movement direction pick ──
    _dir_timer -= 0.016f; // ~1 frame
    if (_dir_timer <= 0 || _current_dir < 0) {
        _pick_direction(player, monsters);
        _dir_timer = 0.3f + (float)(rng() % 100) / 200.0f; // 0.3-0.8s
    }
    bool is_move = false;
    if (_current_dir == 0 && strcmp(action_name, "move_up") == 0) is_move = true;
    if (_current_dir == 1 && strcmp(action_name, "move_down") == 0) is_move = true;
    if (_current_dir == 2 && strcmp(action_name, "move_left") == 0) is_move = true;
    if (_current_dir == 3 && strcmp(action_name, "move_right") == 0) is_move = true;
    if (is_move) return _frame % 2 == 0; // ~30 checks/sec

    // ── Attack: if enemy in range ──
    if (strcmp(action_name, "attack") == 0) {
        Monster* t = _find_nearest(player, monsters);
        if (t) {
            float d = hypotf(t->entity.rect.x - player->entity.rect.x,
                             t->entity.rect.y - player->entity.rect.y);
            if (d < 2.0f * 32.0f) return _frame % 12 == 0; // ~5/sec
        }
        return false;
    }

    // ── Skills: random use ──
    if (strcmp(action_name, "skill_1") == 0 || strcmp(action_name, "skill_2") == 0 ||
        strcmp(action_name, "skill_3") == 0 || strcmp(action_name, "skill_4") == 0) {
        Monster* t = _find_nearest(player, monsters);
        if (t) return _frame % 90 == (int)(rng() % 4) * 20; // ~every 1.5s, random skill
        return false;
    }

    // ── Pickup/interact: always when near special rooms ──
    if (strcmp(action_name, "pickup") == 0) {
        if (map) {
            // check if near any special room tile
            for (auto& sr : map->special_rooms) {
                float dx = player->entity.rect.x + player->entity.rect.width/2 - (sr.cx * 32 + 16);
                float dy = player->entity.rect.y + player->entity.rect.height/2 - (sr.cy * 32 + 16);
                if (sqrtf(dx*dx+dy*dy) < 3.0f * 32.0f)
                    return _frame % 15 == 0;
            }
        }
        return false;
    }

    return false;
}
