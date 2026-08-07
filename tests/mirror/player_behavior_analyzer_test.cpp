// PlayerBehaviorAnalyzer 统计层 — 死字段/语义修复回归测试 (2026-08-07)
#include <gtest/gtest.h>
#include "ai/player_behavior/player_behavior_analyzer.h"
#include "ai/player_behavior/player_habit_profile.h"

static PlayerAction analyze_action(PlayerActionType type, float hp, float dist,
    int floor, int value) {
    PlayerAction a;
    a.type = type;
    a.hp = hp;
    a.enemy_dist = dist;
    a.floor = floor;
    a.value = value;
    return a;
}

TEST(PlayerBehaviorAnalyzer, AverageDistanceComputedFromStream) {
    std::vector<PlayerAction> h;
    h.push_back(analyze_action(PlayerActionType::ATTACK, 1.0f, 6.0f, 5, 0));
    h.push_back(analyze_action(PlayerActionType::ATTACK, 0.9f, 10.0f, 5, 0));
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    // 平均距敌 (6+10)/2 格 × 32px = 256px
    EXPECT_NEAR(p.average_distance, 256.0f, 0.5f);
}

TEST(PlayerBehaviorAnalyzer, AverageDistancePixelUnit) {
    std::vector<PlayerAction> h;
    h.push_back(analyze_action(PlayerActionType::DODGE, 1.0f, 2.0f, 3, 0));
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_NEAR(p.average_distance, 64.0f, 0.5f);   // 2 tiles * 32
}

TEST(PlayerBehaviorAnalyzer, AverageDamagePerFloorIsDamageNotCount) {
    std::vector<PlayerAction> h;
    // 3 次受伤, 总计 90 点, 打到第 3 层 → 平均 30/层
    h.push_back(analyze_action(PlayerActionType::TAKE_DAMAGE, 0.9f, 3.0f, 1, 40));
    h.push_back(analyze_action(PlayerActionType::TAKE_DAMAGE, 0.7f, 3.0f, 2, 30));
    h.push_back(analyze_action(PlayerActionType::TAKE_DAMAGE, 0.5f, 3.0f, 3, 20));
    h.push_back(analyze_action(PlayerActionType::FLOOR_ENTER, 0.5f, 3.0f, 3, 0));
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_NEAR(p.avg_damage_taken, 30.0f, 0.5f);   // 90 / 3 层, 不是 3次/3层
}

TEST(PlayerBehaviorAnalyzer, EmptyStreamSafe) {
    auto p = PlayerBehaviorAnalyzer::analyze({});
    EXPECT_EQ(p.total_actions, 0);
}