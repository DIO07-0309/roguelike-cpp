#include "entity.h"

Entity::Entity(float x, float y, float w, float h)
    : position{x, y}, size{w, h}, collision_size{w, h} {
    sync_rect();
}

Entity::Entity(float x, float y, float vw, float vh, float cw, float ch)
    : position{x, y}, size{vw, vh}, collision_size{cw, ch} {
    sync_rect();
}

void Entity::sync_rect() {
    float cx = position.x + (size.x - collision_size.x) / 2.0f;
    float cy = position.y + (size.y - collision_size.y) / 2.0f;
    rect = {cx, cy, collision_size.x, collision_size.y};
}

Rectangle Entity::draw_rect(float cam_x, float cam_y) const {
    return {position.x - cam_x, position.y - cam_y, size.x, size.y};
}
