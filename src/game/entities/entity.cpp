#include "entity.h"

Entity::Entity(float x, float y, float w, float h)
    : position{x, y}, size{w, h} {
    sync_rect();
}

void Entity::sync_rect() {
    rect = {position.x, position.y, size.x, size.y};
}

Rectangle Entity::draw_rect(float cam_x, float cam_y) const {
    return {position.x - cam_x, position.y - cam_y, size.x, size.y};
}
