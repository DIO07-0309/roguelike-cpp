#pragma once
#include "raylib.h"

// ============================================================
// Entity — 所有游戏对象的基类 (位置 + 碰撞框)
// ============================================================
enum class Direction { DOWN, UP, LEFT, RIGHT };

class Entity {
public:
    Entity(float x, float y, float w, float h);
    Entity(float x, float y, float vw, float vh, float cw, float ch);

    void sync_rect();

    Vector2 position;
    Vector2 size;             // 视觉尺寸
    Vector2 collision_size;   // 碰撞尺寸
    Rectangle rect;           // 碰撞框 (由 collision_size 居中构建)

    Rectangle draw_rect(float cam_x, float cam_y) const;
};
