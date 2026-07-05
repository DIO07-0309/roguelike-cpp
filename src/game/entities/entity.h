#pragma once
#include "raylib.h"

// ============================================================
// Entity — 所有游戏对象的基类 (位置 + 碰撞框)
// ============================================================
enum class Direction { DOWN, UP, LEFT, RIGHT };

class Entity {
public:
    Entity(float x, float y, float w, float h)
        : position{x, y}, size{w, h} {
        sync_rect();
    }

    void sync_rect() {
        rect = {position.x, position.y, size.x, size.y};
    }

    Vector2 position;
    Vector2 size;
    Rectangle rect;

    // 摄像机偏移后的绘制矩形
    Rectangle draw_rect(float cam_x, float cam_y) const {
        return {position.x - cam_x, position.y - cam_y, size.x, size.y};
    }
};
