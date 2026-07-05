#pragma once
#include <string>
#include <memory>
#include <vector>
#include "entity.h"
#include "item.h"

class Player;

// ============================================================
// Effect — 攻击特效数据结构
// ============================================================
struct Effect {
    std::string kind;  // "pulse", "circle", "cone", "spark", "bolt", "flash", "slash_arc"
    float world_x, world_y;
    float radius = 32;
    Color color{255, 200, 50, 255};
    float duration = 0.35f;
    float elapsed = 0.0f;
    Direction direction = Direction::DOWN;  // for slash_arc
    float target_x = 0, target_y = 0;       // for bolt
    int level = 1;
};

// ============================================================
// VFXServer — 特效管理 (单例)
// ============================================================
class VFXServer {
public:
    std::vector<Effect> effects;

    void update(float dt);
    void draw(float cam_x, float cam_y) const;

    void player_attack(float cx, float cy, float range);
    void slash_skill(float cx, float cy, Direction dir, int level);
    void fireball(float cx, float cy, float tx, float ty, int level);
    void heal(float cx, float cy, int level);
    void monster_attack(float mx, float my, float px, float py, Color c);
    void boss_cone(float cx, float cy);
    void boss_circle(float cx, float cy);
    void boss_summon(float cx, float cy);
    void time_stop(float cx, float cy);
    void hit_flash(float x, float y, float size);

private:
    void _sparks(float cx, float cy, int n, Color c, float dur);
};
