#pragma once
#include "entities/entity.h"
#include "world/game_map.h"
#include <cmath>

// 将位移限制到墙体边界：位移到墙边停下，而非卡入墙体
// 返回实际位移量 (可能被截断)
inline Vector2 clamp_displacement(Entity& e, float dx, float dy, GameMap* map) {
    if (!map) return {dx, dy};
    float orig_x = e.position.x;
    float orig_y = e.position.y;

    // 先尝试完整位移
    e.position.x += dx;
    e.position.y += dy;
    e.sync_rect();
    if (map->is_rect_walkable(e.rect)) return {dx, dy};

    // 完整位移卡墙 → 二分搜索最大安全距离
    e.position.x = orig_x;
    e.position.y = orig_y;
    float total = sqrtf(dx * dx + dy * dy);
    if (total < 0.5f) return {0, 0};
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 6; i++) {
        float mid = (lo + hi) * 0.5f;
        e.position.x = orig_x + dx * mid;
        e.position.y = orig_y + dy * mid;
        e.sync_rect();
        if (map->is_rect_walkable(e.rect)) lo = mid; else hi = mid;
    }
    float safe = lo * total;
    float ratio = safe / total;
    e.position.x = orig_x + dx * ratio;
    e.position.y = orig_y + dy * ratio;
    e.sync_rect();
    return {dx * ratio, dy * ratio};
}
