// M1: BehaviorCloneTable — state->intent cloning tests
#include <gtest/gtest.h>
#include "ai/mirror/behavior_clone_table.h"
#include "ai/mirror/mirror_agent.h"

static PlayerAction make_action(PlayerActionType type, float hp, float dist,
    int mask) {
    PlayerAction a;
    a.type = type;
    a.hp = hp;
    a.enemy_dist = dist;
    a.skill_ready_mask = mask;
    return a;
}

// ── Context/key interpretability ──────────────────────────
TEST(CloneContext, KeyIsReadable) {
    CloneContext c = CloneContext::from_state(1.5f, 0.2f, 3);
    EXPECT_EQ(c.key(), "d0:h0:s3");   // close + critical + 3 skills
    EXPECT_EQ(c.fuzzy_key(), "d0:h0");
}

TEST(CloneContext, BucketBoundaries) {
    EXPECT_EQ(CloneContext::from_state(2.0f, 0.25f, 0).key(), "d1:h0:s0");
    EXPECT_EQ(CloneContext::from_state(8.0f, 0.8f, 9).key(), "d3:h3:s3");
}

// ── Table learning ────────────────────────────────────────
TEST(BehaviorClone, LearnsHealForLowHpCloseEnemy) {
    BehaviorCloneTable t;
    CloneContext lowHpClose = CloneContext::from_state(1.5f, 0.2f, 3);
    for (int i = 0; i < 5; i++) t.record_decision(lowHpClose, PlayerIntention::HEAL);
    t.record_decision(lowHpClose, PlayerIntention::ATTACK);   // 1 noisy label

    ClonePrediction p = t.predict(1.5f, 0.2f, 3);
    EXPECT_EQ(p.best, PlayerIntention::HEAL);
    EXPECT_EQ(p.level, 0);                                    // exact hit
    EXPECT_NEAR(p.confidence, 5.0f / 6.0f, 0.01f);
}

TEST(BehaviorClone, BuildsFromPlayerActionStream) {
    std::vector<PlayerAction> stream;
    // Player habit: when low HP + close + skill ready -> heal, otherwise attack
    for (int i = 0; i < 6; i++)
        stream.push_back(make_action(PlayerActionType::HEAL, 0.2f, 1.2f, 3));
    for (int i = 0; i < 4; i++)
        stream.push_back(make_action(PlayerActionType::ATTACK, 0.9f, 1.2f, 3));
    stream.push_back(make_action(PlayerActionType::FLOOR_ENTER, 1.0f, -1, 0));

    BehaviorCloneTable table;
    table.build(stream);
    EXPECT_EQ(table.entries(), 2u);

    ClonePrediction p = table.predict(1.2f, 0.2f, 3);   // exact match
    EXPECT_EQ(p.best, PlayerIntention::HEAL);
    EXPECT_EQ(p.level, 0);

    ClonePrediction pf = table.predict(1.2f, 0.2f, 1);  // unknown skill count -> fuzzy
    EXPECT_EQ(pf.best, PlayerIntention::HEAL);
    EXPECT_EQ(pf.level, 1);
}

// ── Degradation chain ─────────────────────────────────────
TEST(BehaviorClone, DegradesExactToFuzzy) {
    CloneContext a = CloneContext::from_state(2.5f, 0.6f, 0);
    CloneContext b = CloneContext::from_state(2.5f, 0.6f, 2);
    CloneContext miss = CloneContext::from_state(2.5f, 0.6f, 1);  // unknown skill count
    BehaviorCloneTable table;
    table.record_decision(a, PlayerIntention::ATTACK);
    table.record_decision(a, PlayerIntention::ATTACK);
    table.record_decision(b, PlayerIntention::SKILL);

    ClonePrediction p = table.predict(2.5f, 0.6f, 1);
    EXPECT_EQ(p.level, 1);                        // fuzzy (skill merged)
    EXPECT_EQ(p.best, PlayerIntention::ATTACK);   // 2 votes vs 1
}

TEST(BehaviorClone, FallsBackToProfile) {
    PlayerHabitProfile profile;
    profile.heal_frequency = 0.5f;
    profile.hp_counter_threshold = 40.0f;         // heals below 40% HP

    BehaviorCloneTable table;
    table.set_profile(profile);
    ClonePrediction p = table.predict(3.0f, 0.2f, 1);   // nothing trained yet
    EXPECT_EQ(p.level, 2);
    EXPECT_EQ(p.best, PlayerIntention::HEAL);
}

TEST(BehaviorClone, DefaultsToAttack) {
    BehaviorCloneTable table;
    ClonePrediction p = table.predict(5.0f, 0.5f, 0);
    EXPECT_EQ(p.level, 3);
    EXPECT_EQ(p.best, PlayerIntention::ATTACK);
}

// ── MirrorAgent integration (user acceptance test) ────────
TEST(MirrorAgent, PredictsHealFromCloneTable) {
    PlayerHabitProfile profile;
    profile.heal_frequency = 0.0f;                // rules alone would NOT heal here

    MirrorAgent agent;
    agent.init(profile);

    auto clone = std::make_unique<BehaviorCloneTable>();
    CloneContext lowHpClose = CloneContext::from_state(1.4f, 0.2f, 3);
    for (int i = 0; i < 5; i++) clone->record_decision(lowHpClose, PlayerIntention::HEAL);
    clone->record_decision(lowHpClose, PlayerIntention::SKILL);
    agent.set_clone_table(std::move(clone));
    agent.set_phase(2);

    MirrorBattleState st;
    st.player_hp_pct = 0.2f;
    st.dist_tiles = 1.4f;
    st.player_skills_ready = 2;
    EXPECT_EQ(agent.predict_next_action(st), PlayerActionType::HEAL);
}

TEST(MirrorAgent, CloneLayerIgnoredBeforePhase2) {
    PlayerHabitProfile profile;
    profile.heal_frequency = 0.0f;
    MirrorAgent agent;
    agent.init(profile);
    auto clone = std::make_unique<BehaviorCloneTable>();
    clone->record_decision(CloneContext::from_state(1.4f, 0.2f, 3), PlayerIntention::HEAL);
    for (int i = 0; i < 5; i++)
        clone->record_decision(CloneContext::from_state(1.4f, 0.2f, 3), PlayerIntention::HEAL);
    agent.set_clone_table(std::move(clone));
    agent.set_phase(1);                           // observe phase

    MirrorBattleState st;
    st.player_hp_pct = 0.2f;
    st.dist_tiles = 1.4f;
    EXPECT_EQ(agent.predict_next_action(st), PlayerActionType::ATTACK);  // rules fallback
}