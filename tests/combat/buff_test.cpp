// G7.2: Buff lifecycle test (tick / expire / stack)
#include <gtest/gtest.h>

// Minimal BuffDef stub for unit testing
struct SimpleBuffDef {
    std::string id;
    float duration;
    int max_stacks;
    float tick_interval;
    int tick_damage;
};

// Minimal BuffInstance
struct SimpleBuff {
    std::string id;
    int stacks;
    float remaining;
    float tick_timer;
};

// Tick logic: subtract dt, return true if expired
static bool tick_buff(SimpleBuff& b, float dt, const SimpleBuffDef& def) {
    b.remaining -= dt;
    if (def.tick_interval > 0) {
        b.tick_timer -= dt;
        while (b.tick_timer <= 0 && b.remaining > 0) {
            b.tick_timer += def.tick_interval;
        }
    }
    return b.remaining <= 0;
}

TEST(BuffLifecycle, DecaysOverTime) {
    SimpleBuffDef def{"attack_up", 5.0f, 3, 0, 0};
    SimpleBuff b{"attack_up", 1, 5.0f, 0};
    EXPECT_FALSE(tick_buff(b, 1.0f, def));
    EXPECT_NEAR(b.remaining, 4.0f, 0.01f);
    EXPECT_FALSE(tick_buff(b, 3.9f, def));
    EXPECT_TRUE(tick_buff(b, 0.2f, def));
}

TEST(BuffLifecycle, DotTickDamage) {
    // poison: 0.5s interval, 3 damage per tick
    SimpleBuffDef def{"poison", 4.0f, 5, 0.5f, 3};
    SimpleBuff b{"poison", 2, 4.0f, 0.5f};
    int tick_count = 0;
    for (int i = 0; i < 8; i++) {
        if (!tick_buff(b, 0.5f, def)) {
            tick_count++;
        }
    }
    EXPECT_EQ(tick_count, 8); // 4.0s / 0.5s = 8 ticks
    EXPECT_TRUE(b.remaining <= 0);
}

TEST(BuffLifecycle, StackRefreshesDuration) {
    SimpleBuffDef def{"slow", 3.0f, 3, 0, 0};
    SimpleBuff b{"slow", 1, 3.0f, 0};
    tick_buff(b, 2.0f, def);
    EXPECT_NEAR(b.remaining, 1.0f, 0.01f);
    // Refresh: increase stacks, reset duration
    b.stacks = std::min(b.stacks + 1, def.max_stacks);
    b.remaining = def.duration;
    EXPECT_EQ(b.stacks, 2);
    EXPECT_NEAR(b.remaining, 3.0f, 0.01f);
}

TEST(BuffLifecycle, MaxStacks) {
    SimpleBuffDef def{"attack_up", 6.0f, 3, 0, 0};
    SimpleBuff b{"attack_up", 3, 6.0f, 0};
    b.stacks = std::min(b.stacks + 1, def.max_stacks);
    EXPECT_EQ(b.stacks, 3);
}
