// PlayerBehaviorAnalyzer 统计层 — 死字段/语义修复回归测试 (2026-08-07)
#include <gtest/gtest.h>
#include "ai/player_behavior/player_behavior_analyzer.h"
#include "ai/player_behavior/player_habit_profile.h"
#include "game/entities/entity.h"  // Direction

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

// ============================================================
// M5: 条件维度统计 — 受压反击率 / 朝向稳定度 / 攻击节奏方差
// ============================================================

TEST(PlayerBehaviorAnalyzer, FightBackRateCountsPostHitOffense) {
    std::vector<PlayerAction> h;
    // 2 次被击中, 其中 2 次后续动作(带 hit_in_1s=1)反击 → 1.0
    auto dmg = analyze_action(PlayerActionType::TAKE_DAMAGE, 0.8f, 2.0f, 1, 14);
    auto atk = analyze_action(PlayerActionType::ATTACK,      0.6f, 2.0f, 1, 0);
    atk.hit_in_1s = 1;
    auto dmg2 = analyze_action(PlayerActionType::TAKE_DAMAGE, 0.5f, 2.0f, 1, 20);
    auto skl = analyze_action(PlayerActionType::SKILL,        0.3f, 2.0f, 1, 0);
    skl.hit_in_1s = 1;
    h.push_back(dmg); h.push_back(atk); h.push_back(dmg2); h.push_back(skl);
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_NEAR(p.fight_back_rate, 1.0f, 0.01f);
}

TEST(PlayerBehaviorAnalyzer, FightBackRateZeroWhenNoDamage) {
    std::vector<PlayerAction> h;
    auto mov = analyze_action(PlayerActionType::MOVE, 1.0f, 2.0f, 1, 0);
    h.push_back(mov);
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_EQ(p.fight_back_rate, 0.0f);
}

TEST(PlayerBehaviorAnalyzer, FacingStabilityIsDominantRatio) {
    std::vector<PlayerAction> h;
    // 10 个 MOVE, 7 个朝下(3) → 0.7
    for (int i = 0; i < 7; i++) {
        auto m = analyze_action(PlayerActionType::MOVE, 1.0f, 3.0f, 1, 0);
        m.facing_dir = (int)Direction::DOWN; h.push_back(m);
    }
    for (int i = 0; i < 3; i++) {
        auto m = analyze_action(PlayerActionType::MOVE, 1.0f, 3.0f, 1, 1);
        m.facing_dir = (int)Direction::RIGHT; h.push_back(m);
    }
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_NEAR(p.face_enemy_rate, 0.7f, 0.01f);
}

TEST(PlayerBehaviorAnalyzer, AttackRhythmVarianceDetectsRaggedTiming) {
    std::vector<PlayerAction> h;
    // 攻击时间戳: 0,10,11,21 → 间隔 10,1,10 → 方差大 (>1)
    const float ts[4] = { 0.0f, 10.0f, 11.0f, 21.0f };
    for (int i = 0; i < 4; i++) {
        auto a = analyze_action(PlayerActionType::ATTACK, 1.0f, 2.0f, 1, 0);
        a.timestamp = ts[i];
        h.push_back(a);
    }
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_GT(p.attack_rhythm_var, 1.0f);
}

TEST(PlayerBehaviorAnalyzer, AttackRhythmVarianceZeroWithFewSamples) {
    std::vector<PlayerAction> h;
    for (int i = 0; i < 2; i++) {
        auto a = analyze_action(PlayerActionType::ATTACK, 1.0f, 2.0f, 1, 0);
        a.timestamp = (float)i;
        h.push_back(a);
    }
    auto p = PlayerBehaviorAnalyzer::analyze(h);
    EXPECT_EQ(p.attack_rhythm_var, 0.0f);
}