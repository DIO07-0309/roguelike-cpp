#include "vfx_server.h"
#include <cmath>
#include <algorithm>

void VFXServer::update(float dt) {
    for (auto& e : effects) e.elapsed += dt;
    effects.erase(std::remove_if(effects.begin(), effects.end(),
        [](auto& e) { return e.elapsed >= e.duration; }), effects.end());
}

void VFXServer::draw(float cam_x, float cam_y) const {
    for (auto& e : effects) {
        float sx = e.world_x - cam_x;
        float sy = e.world_y - cam_y;
        float alpha = 1.0f - e.elapsed / e.duration;
        if (alpha < 0) alpha = 0;

        Color c = e.color;
        c.a = (unsigned char)(c.a * alpha);

        if (e.kind == "pulse" || e.kind == "circle") {
            float r = e.radius * (0.5f + 0.5f * (e.elapsed / e.duration));
            DrawRing({sx, sy}, r * 0.6f, r, 0, 360, 12, c);
        } else if (e.kind == "spark") {
            DrawCircle(sx, sy, e.radius, c);
        } else if (e.kind == "bolt") {
            DrawLineEx({sx, sy}, {e.target_x - cam_x, e.target_y - cam_y}, 2.0f, c);
        } else if (e.kind == "flash") {
            DrawCircle(sx, sy, e.radius, c);
            DrawCircle(sx, sy, e.radius * 1.5f, Fade(c, 0.3f));
        }
    }
}

void VFXServer::_sparks(float cx, float cy, int n, Color c, float dur) {
    for (int i = 0; i < n; i++) {
        float angle = (float)(rng() % 360) * DEG2RAD;
        float dist = 5 + (float)(rng() % 20);
        effects.push_back({"spark", cx + cosf(angle) * dist,
                           cy + sinf(angle) * dist, 2.0f + (float)(rng() % 4),
                           c, dur, 0});
    }
}

void VFXServer::player_attack(float cx, float cy, float range,
                               const AttackEvolutionState& evo) {
    // G1: Lv1=铁剑(白金色), Lv2=剑气(橙色加宽), Lv3=旋风斩(金色更宽更多粒子)
    if (evo.level == 1) {
        effects.push_back({"pulse", cx, cy, range, {255, 200, 50, 200}, 0.35f});
        _sparks(cx, cy, 4, {255, 220, 100, 255}, 0.25f);
    } else if (evo.level == 2) {
        effects.push_back({"pulse", cx, cy, range * 1.3f, {255, 150, 40, 220}, 0.40f});
        _sparks(cx, cy, 6, {255, 180, 60, 255}, 0.30f);
    } else { // Lv3+
        effects.push_back({"pulse", cx, cy, range * 1.6f, {255, 200, 40, 240}, 0.45f});
        _sparks(cx, cy, 10, {255, 220, 80, 255}, 0.35f);
        // 额外微量震动 (视觉增强)
        effects.push_back({"flash", cx, cy, range * 0.8f, {255, 220, 60, 100}, 0.20f});
    }
}

void VFXServer::slash_skill(float cx, float cy, Direction dir, int level) {
    float r = level == 1 ? 56.0f : level == 2 ? 72.0f : 88.0f;
    effects.push_back({"slash_arc", cx, cy, r, {255, 80, 80, 200}, 0.35f, 0, dir});
    if (level >= 3) _sparks(cx, cy, 8, {255, 120, 80, 255}, 0.35f);
}

void VFXServer::fireball(float cx, float cy, float tx, float ty, int level) {
    effects.push_back({"bolt", cx, cy, 0, {255, 100, 50, 200}, 0.4f, 0,
                       Direction::DOWN, tx, ty, level});
    for (int i = 0; i < level; i++)
        effects.push_back({"pulse", tx, ty, 16.0f + i * 8.0f,
                           {255, 180, 60, 150}, 0.35f + i * 0.1f});
}

void VFXServer::heal(float cx, float cy, int) {
    effects.push_back({"pulse", cx, cy, 40, {100, 255, 100, 180}, 0.5f});
    _sparks(cx, cy, 6, {150, 255, 150, 255}, 0.4f);
}

void VFXServer::monster_attack(float mx, float my, float px, float py, Color c) {
    effects.push_back({"bolt", mx, my, 0, c, 0.25f, 0,
                       Direction::DOWN, px, py});
}

void VFXServer::boss_cone(float cx, float cy) {
    effects.push_back({"pulse", cx, cy, 96, {200, 40, 40, 200}, 0.5f});
}

void VFXServer::boss_circle(float cx, float cy) {
    effects.push_back({"pulse", cx, cy, 80, {220, 50, 50, 200}, 0.5f});
}

void VFXServer::boss_summon(float cx, float cy) {
    effects.push_back({"pulse", cx, cy, 64, {150, 50, 200, 200}, 0.5f});
}

void VFXServer::time_stop(float cx, float cy) {
    effects.push_back({"pulse", cx, cy, 200, {180, 180, 200, 150}, 0.8f});
}

void VFXServer::hit_flash(float x, float y, float size) {
    effects.push_back({"flash", x, y, size * 0.8f, {255, 255, 255, 200}, 0.15f});
}
