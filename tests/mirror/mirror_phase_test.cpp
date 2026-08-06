#include "ai/mirror/mirror_agent.h"
#include "ai/mirror/rolling_accuracy.h"
#include <gtest/gtest.h>

namespace {

// M2: 滚动窗口 — 满 32 滑动
TEST(RollingAccuracy, SlidingWindowAverage) {
    RollingAccuracy acc;
    for (int i = 0; i < 10; ++i) acc.add(true);
    EXPECT_FLOAT_EQ(acc.accuracy(), 1.0f);
    EXPECT_EQ(acc.total(), 10);
    for (int i = 0; i < 5; ++i) acc.add(false);
    EXPECT_FLOAT_EQ(acc.accuracy(), 10.0f / 15.0f);
}

TEST(RollingAccuracy, DropsOldestBeyondCap) {
    RollingAccuracy acc;
    for (int i = 0; i < 32; ++i) acc.add(true);
    for (int i = 0; i < 8; ++i) acc.add(false);
    EXPECT_FLOAT_EQ(acc.accuracy(), 24.0f / 32.0f);
}

TEST(RollingAccuracy, ResetClearsState) {
    RollingAccuracy acc;
    for (int i = 0; i < 8; ++i) acc.add(true);
    acc.reset();
    EXPECT_FLOAT_EQ(acc.accuracy(), 0.0f);
    EXPECT_EQ(acc.total(), 0);
}

void FeedCorrectSequence(MirrorAgent& agent, int count) {
    for (int i = 0; i < count; ++i) {
        agent.on_prediction(PlayerActionType::HEAL, 1.2f, 0.2f, 2);
        agent.observe_actual(PlayerActionType::HEAL);
    }
}

// M2: 动态阶段触发
TEST(DynamicPhase, ObservationCountBackstopPromotesToPhase2) {
    MirrorAgent agent;
    EXPECT_EQ(agent.current_phase(), 1);
    FeedCorrectSequence(agent, 40);
    agent.tick_phase(0.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 2);
}

TEST(DynamicPhase, RollingAccuracyPromotesToPhase2) {
    MirrorAgent agent;
    FeedCorrectSequence(agent, 20);
    agent.tick_phase(0.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 2);
}

TEST(DynamicPhase, LowAccuracyKeepsObserving) {
    MirrorAgent agent;
    for (int i = 0; i < 25; ++i) {
        agent.on_prediction(PlayerActionType::ATTACK, 5.0f, 0.9f, 0);
        agent.observe_actual(PlayerActionType::HEAL);
    }
    agent.tick_phase(0.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 1);
}

TEST(DynamicPhase, TimeBackstopPromotesToPhase2) {
    MirrorAgent agent;
    agent.tick_phase(13.0f, MirrorBattleState{});   // M4: 兜底 20→12s
    EXPECT_EQ(agent.current_phase(), 2);
}

TEST(DynamicPhase, TuningCanExtendTimeBackstop) {
    auto& t = mirror_tuning();
    t.phase1_time_backstop = 99.0f;   // 拉长兜底 → 不触发
    MirrorAgent agent;
    agent.tick_phase(13.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 1);
    t.phase1_time_backstop = 12.0f;   // 恢复默认, 避免影响其他用例
}

TEST(DynamicPhase, CorePatternPromotesToPhase3) {
    MirrorAgent agent;
    agent.set_phase(2);
    FeedCorrectSequence(agent, 10);   // 同桶 10 命中, acc=1.0 ≥0.7
    agent.tick_phase(0.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 3);
}

TEST(DynamicPhase, DangerPromotesToPhase3) {
    MirrorAgent agent;
    agent.set_phase(2);
    MirrorBattleState st;
    st.player_hp_pct = 0.2f;
    agent.tick_phase(0.0f, st);
    EXPECT_EQ(agent.current_phase(), 3);
}

TEST(DynamicPhase, Phase3IsStable) {
    MirrorAgent agent;
    agent.set_phase(3);
    agent.tick_phase(1.0f, MirrorBattleState{});
    EXPECT_EQ(agent.current_phase(), 3);
}

// M2: profile_drift — 画像频率 vs 当前战斗频率
TEST(ProfileDrift, NoDataMeansZero) {
    MirrorAgent agent;
    EXPECT_FLOAT_EQ(agent.profile_drift(), 0.0f);
}

TEST(ProfileDrift, DeviatesFromProfiledFrequency) {
    PlayerHabitProfile prof;
    prof.attack_frequency = 2.0f;
    prof.skill_frequency  = 1.0f;
    MirrorAgent agent;
    agent.init(prof);
    agent.tick_phase(10.0f, MirrorBattleState{});
    for (int i = 0; i < 30; ++i) {
        agent.on_prediction(PlayerActionType::ATTACK, 5.0f, 0.9f, 0);
        agent.observe_actual(PlayerActionType::ATTACK);
    }
    // 当前 3.0/s vs 2.0 → 0.5; 技能 0 vs 1.0 → 1.0; drift=(0.5+1.0)/2
    EXPECT_NEAR(agent.profile_drift(), 0.75f, 0.01f);
}

// M4: 漂移降权 — 玩家换打法 → 克隆置信门槛上浮 (模仿降权)
TEST(M4DriftPenalty, HighDriftRaisesConfidenceBar) {
    PlayerHabitProfile prof;
    prof.attack_frequency = 2.0f;
    prof.skill_frequency  = 1.0f;
    MirrorAgent agent;
    agent.init(prof);
    agent.tick_phase(10.0f, MirrorBattleState{});
    for (int i = 0; i < 30; ++i) {
        agent.on_prediction(PlayerActionType::ATTACK, 5.0f, 0.9f, 0);
        agent.observe_actual(PlayerActionType::ATTACK);
    }
    EXPECT_GT(agent.profile_drift(), 0.5f);              // 漂移已超惩罚阈值
    EXPECT_FLOAT_EQ(agent.clone_confidence_threshold(), 0.75f);  // 0.5+0.25
}

TEST(M4DriftPenalty, LowDriftKeepsBaseBar) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    EXPECT_FLOAT_EQ(agent.clone_confidence_threshold(), 0.50f);   // 无漂移 → 门槛不变
}

}  // namespace