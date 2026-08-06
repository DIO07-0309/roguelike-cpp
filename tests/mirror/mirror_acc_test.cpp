#include "ai/mirror/mirror_debug_stats.h"
#include "ai/mirror/mirror_agent.h"
#include <gtest/gtest.h>

namespace {

// 验收: 统计逻辑单元 — 证明调用链每个环节都有计数出口
TEST(MirrorDebugStats, ResetClearsAllCounters) {
    MirrorDebugStats s;
    s.on_predict(0);
    s.on_interrupt(true);
    s.tick_phase(1, 3.0f);
    s.reset();
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_EQ(snap.predict_count, 0);
    EXPECT_EQ(snap.interrupt_attempt, 0);
    EXPECT_FLOAT_EQ(snap.phase_seconds[0], 0.0f);
}

TEST(MirrorDebugStats, PredictLevelsAreTracked) {
    MirrorDebugStats s;
    s.on_predict(0);   // clone exact
    s.on_predict(1);   // clone fuzzy
    s.on_predict(2);   // profile
    s.on_predict(3);   // default
    s.on_predict(-1);  // rule fallback
    s.on_predict(0);
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_EQ(snap.predict_count, 6);
    EXPECT_EQ(snap.clone_exact, 2);
    EXPECT_EQ(snap.clone_fuzzy, 1);
    EXPECT_EQ(snap.profile_fallback, 1);
    EXPECT_EQ(snap.default_fallback, 1);
    EXPECT_EQ(snap.rule_fallback, 1);
}

TEST(MirrorDebugStats, ArbitrateSourcesAreTracked) {
    MirrorDebugStats s;
    s.on_arbitrate(true, false);    // clone
    s.on_arbitrate(false, true);    // ml
    s.on_arbitrate(false, false);   // thompson
    s.on_arbitrate(true, false);
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_EQ(snap.clone_arbitrate, 2);
    EXPECT_EQ(snap.ml_slot_used, 1);
    EXPECT_EQ(snap.thompson_used, 1);
}

TEST(MirrorDebugStats, InterruptAttemptAndSuccess) {
    MirrorDebugStats s;
    s.on_interrupt(false);   // attempt only
    s.on_interrupt(true);    // attempt + success
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_EQ(snap.interrupt_attempt, 2);
    EXPECT_EQ(snap.interrupt_success, 1);
}

TEST(MirrorDebugStats, BehaviorDistributionCounted) {
    MirrorDebugStats s;
    s.on_behavior_state(0);   // approach
    s.on_behavior_state(1);   // attack
    s.on_behavior_state(2);   // skill
    s.on_behavior_state(3);   // retreat
    s.on_behavior_state(1);
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_EQ(snap.behavior_approach, 1);
    EXPECT_EQ(snap.behavior_attack, 2);
    EXPECT_EQ(snap.behavior_skill, 1);
    EXPECT_EQ(snap.behavior_retreat, 1);
}

TEST(MirrorDebugStats, PhaseSecondsAccumulate) {
    MirrorDebugStats s;
    s.tick_phase(1, 2.5f);
    s.tick_phase(2, 3.5f);
    s.tick_phase(3, 1.0f);
    MirrorDebugSnap snap = s.snapshot();
    EXPECT_FLOAT_EQ(snap.phase_seconds[0], 2.5f);
    EXPECT_FLOAT_EQ(snap.phase_seconds[1], 3.5f);
    EXPECT_FLOAT_EQ(snap.phase_seconds[2], 1.0f);
}

TEST(MirrorDebugStats, SummaryReportsKeyFields) {
    MirrorDebugStats s;
    s.on_predict(0);
    s.on_predict(-1);
    s.on_interrupt(true);
    s.tick_phase(2, 5.0f);
    std::string sum = s.summary();
    EXPECT_NE(sum.find("Predict:2"), std::string::npos);
    EXPECT_NE(sum.find("CloneHit"), std::string::npos);
    EXPECT_NE(sum.find("Rule"), std::string::npos);
    EXPECT_NE(sum.find("打断1/1"), std::string::npos);
}

// ── 集成: MirrorAgent 真实代码路径驱动统计 (证明链路接入, 非死代码) ──
TEST(MirrorDebugStatsIntegration, RulePredictIncrementsCounters) {
    PlayerHabitProfile prof;
    prof.attack_frequency = 2.0f;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    MirrorBattleState st;
    st.dist_tiles = 1.0f; st.player_hp_pct = 0.9f;
    agent.predict_next_action(st);   // 无克隆表 → 规则兜底
    MirrorDebugSnap snap = agent.debug_stats()->snapshot();
    EXPECT_EQ(snap.predict_count, 1);
    EXPECT_EQ(snap.rule_fallback, 1);
}

TEST(MirrorDebugStatsIntegration, ClonePredictCountsExact) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    auto table = std::make_unique<BehaviorCloneTable>();
    CloneContext ctx = CloneContext::from_state(1.2f, 0.2f, 2);
    for (int i = 0; i < 9; ++i)
        table->record_decision(ctx, PlayerIntention::HEAL);
    agent.set_clone_table(std::move(table));
    MirrorBattleState st;
    st.dist_tiles = 1.2f; st.player_hp_pct = 0.2f; st.player_skills_ready = 2;
    EXPECT_EQ(agent.predict_next_action(st), PlayerActionType::HEAL);
    MirrorDebugSnap snap = agent.debug_stats()->snapshot();
    EXPECT_EQ(snap.predict_count, 1);
    EXPECT_EQ(snap.clone_exact, 1);
}

TEST(MirrorDebugStatsIntegration, TickPhaseAccumulatesBattleTime) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    MirrorBattleState st;
    agent.tick_phase(2.0f, st);
    agent.tick_phase(1.5f, st);
    MirrorDebugSnap snap = agent.debug_stats()->snapshot();
    EXPECT_FLOAT_EQ(snap.phase_seconds[0], 3.5f);   // phase 1 时长
    EXPECT_FLOAT_EQ(snap.phase_seconds[1], 0.0f);
}

}  // namespace