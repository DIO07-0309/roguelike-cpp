#include <gtest/gtest.h>
#include <cmath>
#include "systems/dodge_component.h"

TEST(DodgeComponent, RollMovesFullDistanceThenEnds) {
    DodgeComponent d; ASSERT_TRUE(d.try_start({1, 0}));
    float total = 0;
    for (int i = 0; i < 20 && d.active(); i++) { d.tick(1 / 60.f); total += d.delta_this_frame().x; }
    EXPECT_NEAR(total, DodgeComponent::kDistance, 8.0f);   // ±1 帧 (末帧 tick 终止不再计 delta)
    EXPECT_FALSE(d.active());
    EXPECT_NEAR(d.remaining_cd(), DodgeComponent::kCooldown - DodgeComponent::kDuration, 0.03f);
}

TEST(DodgeComponent, CooldownRejectsThenAllows) {
    DodgeComponent d; d.try_start({1, 0});
    for (int i = 0; i < 12; i++) d.tick(1 / 60.f);           // 0.2s: 翻滚已完, cd 还剩 0.5
    EXPECT_FALSE(d.try_start({1, 0}));
    for (int i = 0; i < 36; i++) d.tick(1 / 60.f);           // 累计 0.8s > cd
    EXPECT_TRUE(d.try_start({1, 0}));
}

TEST(DodgeComponent, RejectsWhileActive) {
    DodgeComponent d; d.try_start({0, 1}); d.tick(1 / 60.f);
    EXPECT_FALSE(d.try_start({0, 1}));
}

TEST(DodgeComponent, TiltSquashReturnToNeutral) {
    DodgeComponent d; d.try_start({1, 0}); d.tick(1 / 60.f); d.tick(1 / 60.f);
    EXPECT_NE(d.tilt_deg(), 0.0f);
    Vector2 s = d.squash_scale(); EXPECT_GT(s.x, 1.0f); EXPECT_LT(s.y, 1.0f);
    for (int i = 0; i < 20; i++) d.tick(1 / 60.f);
    EXPECT_FLOAT_EQ(d.tilt_deg(), 0.0f);
    Vector2 n = d.squash_scale(); EXPECT_FLOAT_EQ(n.x, 1.0f); EXPECT_FLOAT_EQ(n.y, 1.0f);
}

TEST(DodgeComponent, GhostCapAndDecay) {
    DodgeComponent d; d.try_start({1, 0});
    for (int i = 0; i < 4; i++) d.push_ghost({(float)i * 8, 0});
    ASSERT_EQ(d.ghosts().size(), 3u);                        // 上限 3, 留最新
    d.tick(0.2f);                                            // 全部超龄
    EXPECT_TRUE(d.ghosts().empty());
}

TEST(DodgeComponent, ResetClearsAll) {
    DodgeComponent d; d.try_start({1, 0}); d.tick(1 / 60.f); d.reset();
    EXPECT_FALSE(d.active()); EXPECT_FLOAT_EQ(d.remaining_cd(), 0.0f); EXPECT_TRUE(d.ghosts().empty());
    EXPECT_TRUE(d.try_start({1, 0}));                        // reset 后立即可翻
}

TEST(RollDirection, AxisPriorityAndFacingFallback) {
    Vector2 a = roll_direction({1, 1}, Direction::UP);       // 斜轴归一化
    EXPECT_NEAR(a.x, 0.707f, 0.01f); EXPECT_NEAR(a.y, 0.707f, 0.01f);
    Vector2 f = roll_direction({0, 0}, Direction::LEFT);     // 无输入 → 面朝
    EXPECT_FLOAT_EQ(f.x, -1.0f); EXPECT_FLOAT_EQ(f.y, 0.0f);
}
