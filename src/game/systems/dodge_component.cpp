#include "systems/dodge_component.h"
#include <cmath>

bool DodgeComponent::try_start(const Vector2& dir_norm) {
    if (_running || _cd > 0.0f) return false;
    _dir = dir_norm; _running = true; _elapsed = 0.0f; _frame = 0; _cd = kCooldown;
    return true;
}

void DodgeComponent::tick(float dt) {
    _dt = dt;
    if (_cd > 0.0f) _cd -= dt;
    for (auto& g : _ghosts) g.age += dt;
    while (!_ghosts.empty() && _ghosts.front().age >= kGhostLife) _ghosts.erase(_ghosts.begin());
    if (!_running) return;
    _elapsed += dt; _frame++;
    if (_elapsed >= kDuration) { _running = false; _elapsed = 0.0f; _frame = 0; }
}

float DodgeComponent::tilt_deg() const {
    if (!_running) return 0.0f;
    float k = sinf(_elapsed / kDuration * 3.1415926f);
    float sign = _dir.x > 0.0f ? 1.0f : _dir.x < 0.0f ? -1.0f : 1.0f;
    return 12.0f * k * sign;
}

Vector2 DodgeComponent::squash_scale() const {
    if (!_running) return {1.0f, 1.0f};
    float k = sinf(_elapsed / kDuration * 3.1415926f);
    return {1.0f + 0.10f * k, 1.0f - 0.15f * k};
}

void DodgeComponent::push_ghost(const Vector2& pos) {
    if (_ghosts.size() >= 3) _ghosts.erase(_ghosts.begin());
    _ghosts.push_back({pos, 0.0f});
}

void DodgeComponent::reset() { *this = DodgeComponent{}; }

Vector2 roll_direction(const Vector2& axis, Direction facing) {
    float len = sqrtf(axis.x * axis.x + axis.y * axis.y);
    if (len > 0.1f) return {axis.x / len, axis.y / len};
    switch (facing) {
        case Direction::UP:    return {0.0f, -1.0f};
        case Direction::DOWN:  return {0.0f,  1.0f};
        case Direction::LEFT:  return {-1.0f, 0.0f};
        case Direction::RIGHT: return { 1.0f, 0.0f};
    }
    return {0.0f, 1.0f};
}
