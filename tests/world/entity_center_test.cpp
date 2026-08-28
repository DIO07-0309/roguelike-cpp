#include <gtest/gtest.h>
#include "entities/entity.h"
#include "entities/player.h"

TEST(EntityCenterInvariant, VisualCenterEqualsCollisionCenter) {
    Entity e(100.0f, 200.0f, 32.0f, 32.0f, 28.0f, 28.0f);
    float visual_cx = e.position.x + e.size.x / 2.0f;
    float visual_cy = e.position.y + e.size.y / 2.0f;
    float collision_cx = e.rect.x + e.rect.width / 2.0f;
    float collision_cy = e.rect.y + e.rect.height / 2.0f;
    EXPECT_FLOAT_EQ(visual_cx, collision_cx);
    EXPECT_FLOAT_EQ(visual_cy, collision_cy);
}

TEST(EntityCenterInvariant, SyncRectPreservesCenter) {
    Entity e(50.0f, 75.0f, 32.0f, 32.0f, 28.0f, 28.0f);
    float cx_before = e.rect.x + e.rect.width / 2.0f;
    float cy_before = e.rect.y + e.rect.height / 2.0f;
    e.position.x += 10.0f;
    e.position.y += 5.0f;
    e.sync_rect();
    float cx_after = e.rect.x + e.rect.width / 2.0f;
    float cy_after = e.rect.y + e.rect.height / 2.0f;
    EXPECT_FLOAT_EQ(cx_after, cx_before + 10.0f);
    EXPECT_FLOAT_EQ(cy_after, cy_before + 5.0f);
}

TEST(EntityCenterInvariant, PlayerCollisionRectSmallerThanVisual) {
    Player p(0.0f, 0.0f, 200.0f, 100, 10, 5, 5);
    EXPECT_FLOAT_EQ(p.entity.size.x, 32.0f);
    EXPECT_FLOAT_EQ(p.entity.size.y, 32.0f);
    EXPECT_FLOAT_EQ(p.entity.collision_size.x, 28.0f);
    EXPECT_FLOAT_EQ(p.entity.collision_size.y, 28.0f);
    EXPECT_FLOAT_EQ(p.entity.rect.width, 28.0f);
    EXPECT_FLOAT_EQ(p.entity.rect.height, 28.0f);
}

TEST(EntityCenterInvariant, DrawRectUsesVisualSize) {
    Entity e(100.0f, 200.0f, 32.0f, 32.0f, 28.0f, 28.0f);
    Rectangle dr = e.draw_rect(0.0f, 0.0f);
    EXPECT_FLOAT_EQ(dr.width, 32.0f);
    EXPECT_FLOAT_EQ(dr.height, 32.0f);
    EXPECT_FLOAT_EQ(dr.x, 100.0f);
    EXPECT_FLOAT_EQ(dr.y, 200.0f);
}
