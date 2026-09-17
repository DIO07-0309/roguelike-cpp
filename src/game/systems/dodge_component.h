#pragma once
#include "raylib.h"
#include "entities/entity.h"   // Direction
#include <vector>

// B3: 翻滚纯手感组件 — 零 RNG / dt 定步长 / 无无敌帧 (spec §2)
struct RollGhost { Vector2 pos; float age; };

class DodgeComponent {
public:
    static constexpr float kDistance = 64.0f, kDuration = 0.16f, kCooldown = 0.70f;
    static constexpr float kSpeed = kDistance / kDuration;
    static constexpr float kGhostLife = 0.1f;

    bool try_start(const Vector2& dir_norm);
    void tick(float dt);                       // 推进计时/冷却/残影老化
    bool active() const { return _running; }
    Vector2 delta_this_frame() const {
        if (!_running) return {0.0f, 0.0f};
        float s = kSpeed * _dt;
        return {_dir.x * s, _dir.y * s};
    }
    float remaining_cd() const { return _cd > 0 ? _cd : 0; }
    float tilt_deg() const;
    Vector2 squash_scale() const;
    bool ghost_due() const { return _running && (_frame % 2 == 0); }
    void push_ghost(const Vector2& pos);       // 控制器补世界坐标 (rect 左上)
    const std::vector<RollGhost>& ghosts() const { return _ghosts; }
    void reset();

private:
    Vector2 _dir = {0.0f, 1.0f};
    bool  _running = false;
    float _elapsed = 0.0f, _cd = 0.0f, _dt = 0.0f;
    int   _frame = 0;
    std::vector<RollGhost> _ghosts;
};

Vector2 roll_direction(const Vector2& axis, Direction facing);
