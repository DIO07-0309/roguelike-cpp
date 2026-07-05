#include "player.h"
#include "skill.h"
#include "item.h"
#include "config.h"
#include <cmath>
#include <algorithm>

Player::Player(float x, float y, float spd, int hp, int atk, int pdef, int mdef)
    : entity(x, y, 32, 32), speed(spd), combat(hp, atk, pdef, mdef),
      inventory(INVENTORY_MAX) {}

void Player::reset_attack_timers() {
    _last_attack_time = -999.0f;
    for (auto& s : skills.active_skills) {
        s->last_use_time = -999.0f;
    }
    for (auto& s : skills.passives) {
        s->last_use_time = -999.0f;
    }
}

bool Player::can_attack(double game_time) const {
    return (game_time - _last_attack_time) >= ATTACK_COOLDOWN;
}

Vector2 Player::handle_input(const InputMap& input) {
    Vector2 move = input.get_movement_axis();

    if (input.is_action_pressed("move_up") && !input.is_action_pressed("move_down"))
        direction = Direction::UP;
    if (input.is_action_pressed("move_down") && !input.is_action_pressed("move_up"))
        direction = Direction::DOWN;
    if (input.is_action_pressed("move_left") && !input.is_action_pressed("move_right"))
        direction = Direction::LEFT;
    if (input.is_action_pressed("move_right") && !input.is_action_pressed("move_left"))
        direction = Direction::RIGHT;

    is_moving = (move.x != 0 || move.y != 0);
    return move;
}

void Player::give_xp(int amount) { xp += amount; }

void Player::auto_level_to(int target) {
    while (level < target) {
        xp = xp_to_next;
        // give_xp 在 game_scene 中会触发升级
    }
}

void Player::draw_no_cam(float cam_x, float cam_y) {
    Rectangle dr = entity.draw_rect(cam_x, cam_y);

    // 阴影
    DrawEllipse(dr.x + dr.width/2, dr.y + dr.height + 2, dr.width/2 - 2, 3,
                {0, 0, 0, 100});

    // 身体
    Rectangle body = {dr.x + 3, dr.y + 2, dr.width - 6, dr.height - 4};
    DrawRectangleRounded(body, 0.2f, 4, {40, 160, 40, 255});
    DrawRectangleRounded({dr.x + 5, dr.y + 3, dr.width - 10, 8}, 0.15f, 3,
                          {80, 210, 80, 255});

    // 朝向瞳孔
    float cx = dr.x + dr.width / 2, cy = dr.y + dr.height / 2;
    DrawCircle(cx, cy, 5, WHITE);
    float ox = 0, oy = 3;
    switch (direction) {
        case Direction::UP:    oy = -3; break;
        case Direction::DOWN:  oy = 3;  break;
        case Direction::LEFT:  ox = -3; break;
        case Direction::RIGHT: ox = 3;  break;
    }
    DrawCircle(cx + ox, cy + oy, 3, {20, 20, 20, 255});
}
