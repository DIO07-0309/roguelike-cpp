#include "ai/mirror/mirror_agent.h"
#include <gtest/gtest.h>

namespace {

MirrorBattleState LowHpCloseState() {
    MirrorBattleState st;
    st.dist_tiles = 1.2f;
    st.player_hp_pct = 0.2f;
    st.player_skills_ready = 2;
    return st;
}

std::unique_ptr<BehaviorCloneTable> HealBiasedTable(int heal_count, int attack_count) {
    auto table = std::make_unique<BehaviorCloneTable>();
    CloneContext ctx = CloneContext::from_state(1.2f, 0.2f, 2);
    for (int i = 0; i < heal_count; ++i)
        table->record_decision(ctx, PlayerIntention::HEAL);
    for (int i = 0; i < attack_count; ++i)
        table->record_decision(ctx, PlayerIntention::ATTACK);
    return table;
}

// M3: 克隆层高置信度 → 克隆意图驱动行为臂 (HEAL → 压近惩罚治疗)
TEST(CloneArbitration, HighConfidenceDrivesApproach) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    agent.set_clone_table(HealBiasedTable(9, 1));   // conf=0.9 > 0.5
    int arm = agent.recommend_action(LowHpCloseState());
    EXPECT_EQ(arm, (int)MirrorAction::APPROACH);
}

// M3: 克隆层低置信度 → 回落 Thompson 采样
TEST(CloneArbitration, LowConfidenceFallsBackToThompson) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    agent.set_clone_table(HealBiasedTable(4, 6));   // conf=0.4 < 0.5
    int arm = agent.recommend_action(LowHpCloseState());
    EXPECT_GE(arm, 0);                              // Thompson 返回真实臂
    EXPECT_LE(arm, (int)MirrorAction::COMBO);
}

// M3: ML 插槽注册 → 仲裁优先于克隆层
TEST(CloneArbitration, MlSlotOverridesClone) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    agent.set_clone_table(HealBiasedTable(9, 1));   // 本应 HEAL→APPROACH
    agent.set_ml_predictor([](const MirrorBattleState&) {
        return PlayerActionType::ATTACK;            // 外部 ML 预测攻击
    });
    int arm = agent.recommend_action(LowHpCloseState());
    EXPECT_EQ(arm, (int)MirrorAction::COMBO);
}

// M3: ML 插槽默认关闭 (nullptr) → 克隆层照常工作
TEST(CloneArbitration, MlSlotDisabledByDefault) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    agent.set_clone_table(HealBiasedTable(9, 1));
    int arm = agent.recommend_action(LowHpCloseState());
    EXPECT_EQ(arm, (int)MirrorAction::APPROACH);   // 克隆生效, 未被插槽干扰
}

// M3: ML 插槽返回非决策动作 → 忽略并回落克隆层
TEST(CloneArbitration, MlSlotNonDecisionIgnored) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    agent.set_clone_table(HealBiasedTable(9, 1));
    agent.set_ml_predictor([](const MirrorBattleState&) {
        return PlayerActionType::MOVE;              // 非决策 → IDLE → -1
    });
    int arm = agent.recommend_action(LowHpCloseState());
    EXPECT_EQ(arm, (int)MirrorAction::APPROACH);   // 回落克隆层
}

// M3: 观察期 (phase 1) 仲裁不介入 → 规则 (-1)
TEST(CloneArbitration, Phase1StillUsesRules) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_clone_table(HealBiasedTable(9, 1));
    EXPECT_EQ(agent.current_phase(), 1);
    EXPECT_EQ(agent.recommend_action(LowHpCloseState()), -1);
}

}  // namespace