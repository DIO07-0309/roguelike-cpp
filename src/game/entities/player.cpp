#include "player.h"
#include "skill.h"
#include "item.h"
#include "config.h"
#include <cmath>
#include <algorithm>

// ============================================================
// D2: ComboState 实现
// ============================================================
float ComboState::multiplier() const {
    if (count == 4) return 2.2f;
    if (count == 3) return 1.4f;
    if (count == 2) return 1.15f;
    return 1.0f;
}

bool ComboState::is_heavy() const { return count == 4; }

void ComboState::hit(double game_time) {
    if (timer <= 0) count = 0;
    count++;
    if (count > 4) count = 1;
    last_hit_time = (float)game_time;
    timer = WINDOW;
}

void ComboState::tick(float dt) {
    if (timer > 0) timer -= dt;
    if (timer <= 0) { timer = 0; count = 0; }
}

// D2 Step2: 消耗重击 — 技能/Relic/Boss 统一接口
bool Player::consume_heavy_combo() {
    if (!combo.is_heavy()) return false;
    combo.count = 0;  // 重置连击 (已消耗)
    return true;
}

Player::Player(float x, float y, float spd, int hp, int atk, int pdef, int mdef)
    : entity(x, y, 32, 32), speed(spd), combat(hp, atk, pdef, mdef),
      inventory(INVENTORY_MAX) {}

int Player::calc_xp_for_level(int lvl) { return lvl * lvl * 20; }

int Player::attack_target(Player* target, double game_time) {
    (void)target; (void)game_time;
    return 0; // 玩家打怪物用 combat_system
}

void Player::render(Camera2D& cam) { (void)cam; }

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

    // D2: 重击时身体变大
    float heavy_scale = combo.is_heavy() ? 1.25f : 1.0f;
    float hw = dr.width * heavy_scale, hh = dr.height * heavy_scale;
    float hx = dr.x - (hw - dr.width) / 2, hy = dr.y - (hh - dr.height) / 2;

    // 阴影
    DrawEllipse(hx + hw/2, hy + hh + 2, hw/2 - 2, 3, {0, 0, 0, 100});

    // D2: 连击段数 → 身体颜色渐变 (Lv1绿 → Lv4金黄)
    Color body_c =
        combo.count >= 4 ? Color{255, 200, 40, 255} :
        combo.count >= 3 ? Color{80, 200, 80, 255}  :
        combo.count >= 2 ? Color{60, 180, 140, 255} :
                           Color{40, 160, 40, 255};

    Rectangle body_r = {hx + 3, hy + 2, hw - 6, hh - 4};
    DrawRectangleRounded(body_r, 0.2f, 4, body_c);
    DrawRectangleRounded({hx + 5, hy + 3, hw - 10, 8}, 0.15f, 3,
                          {80, 210, 80, 255});

    // 朝向瞳孔
    float cx = hx + hw / 2, cy = hy + hh / 2;
    DrawCircle(cx, cy, 5, WHITE);
    float ox = 0, oy = 3;
    switch (direction) {
        case Direction::UP:    oy = -3; break;
        case Direction::DOWN:  oy = 3;  break;
        case Direction::LEFT:  ox = -3; break;
        case Direction::RIGHT: ox = 3;  break;
    }
    DrawCircle(cx + ox, cy + oy, 3, {20, 20, 20, 255});

    // D2: Combo 指示器 (连击数显示在头顶)
    if (combo.count >= 2 && combo.timer > 0) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", combo.count);
        int fsize = combo.count >= 4 ? 20 : 14;
        Color cc = combo.count >= 4 ? Color{255, 200, 30, 255} : Color{255, 255, 220, 200};
        DrawText(buf, (int)(hx + hw/2 - 4), (int)(hy - 18), fsize, cc);
    }
}
