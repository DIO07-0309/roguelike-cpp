// M4.4 E2E: 全链路真机路径验证 — 采集→画像→克隆→战术链→在线观察→仲裁
// 走真实 PlayerBehaviorRecorder API + g_behavior.history() + MirrorAgent 决策链
#include <gtest/gtest.h>
#include "ai/mirror/mirror_agent.h"
#include "ai/mirror/tactical_chain_table.h"
#include "ai/player_behavior/player_behavior_recorder.h"
#include "ai/player_behavior/player_behavior_analyzer.h"
#include "ai/player_behavior/player_habit_profile.h"

namespace {

TEST(SequenceE2E, FullChainMirrorsPlayerComboRoutine) {
    // ── 1. 采集: 玩家固定套路 "技能0 → 右移 → 连招1 → 连招2" 反复 6 轮 ──
    g_behavior.clear();
    for (int i = 0; i < 6; i++) {
        g_behavior.set_context(0.9f, 3.0f, 0x0F);
        g_behavior.on_skill_use("slash", (float)i * 2.0f, 1, 100.0f, 100.0f);
        g_behavior.on_move_state_change((float)i * 2.0f + 0.5f, 1, 140.0f, 100.0f, 1);
        g_behavior.on_weapon_attack("sword", (float)i * 2.0f + 1.0f, 1,
                                    150.0f, 100.0f, 1, 0);
        g_behavior.on_weapon_attack("sword", (float)i * 2.0f + 1.5f, 1,
                                    150.0f, 100.0f, 1, 1);
    }
    const auto& history = g_behavior.history();
    ASSERT_GE(history.size(), 24u);

    // ── 2. 画像 + 离线构建 (与 boss_system_director F15 注入路径一致) ──
    PlayerHabitProfile prof = PlayerBehaviorAnalyzer::analyze(history);
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);

    auto clone = std::make_unique<BehaviorCloneTable>();
    clone->build(history);
    clone->set_profile(prof);
    agent.set_clone_table(std::move(clone));

    auto chain = std::make_unique<TacticalChainTable>();
    chain->build(history);
    ASSERT_GE(chain->entries(), 1);
    agent.set_chain_table(std::move(chain));

    // ── 3. 在线观察: 玩家又打出 技能→普攻 (类型级符号 SKILL_0, COMBO_1) ──
    agent.observe_actual(PlayerActionType::SKILL);
    agent.observe_actual(PlayerActionType::ATTACK);

    // ── 4. 仲裁: 战术链 3-gram (SKILL_0,COMBO_1)→COMBO_2 高置信 → 连招应对臂 ──
    MirrorBattleState st;
    st.dist_tiles = 2.0f; st.player_hp_pct = 0.8f;
    int arm = agent.recommend_action(st);
    // 玩家套路固定 → 战术链应命中 COMBO 臂 (镜像用连招反制玩家下一段连招)
    EXPECT_EQ(arm, (int)MirrorAction::COMBO);
}

TEST(SequenceChainE2E, RecorderStreamFillsComboStage) {
    g_behavior.clear();
    g_behavior.on_weapon_attack("nunchaku", 1.0f, 1, 50.0f, 50.0f, 3, 2);
    const auto& h = g_behavior.history();
    ASSERT_EQ(h.size(), 1u);
    EXPECT_EQ(h[0].weapon_type, 3);
    EXPECT_EQ(h[0].combo_stage, 2);
    EXPECT_STREQ(h[0].weapon_name, "nunchaku");
}

// 二分定位: analyzer 在 weapon_type 流上是否崩溃
TEST(SequenceChainE2E, AnalyzerSafeOnRecordedStream) {
    g_behavior.clear();
    for (int i = 0; i < 6; i++) {
        g_behavior.set_context(0.9f, 3.0f, 0x0F);
        g_behavior.on_skill_use("slash", (float)i * 2.0f, 1, 100.0f, 100.0f);
        g_behavior.on_move_state_change((float)i * 2.0f + 0.5f, 1, 140.0f, 100.0f, 1);
        g_behavior.on_weapon_attack("sword", (float)i * 2.0f + 1.0f, 1,
                                    150.0f, 100.0f, 1, 0);
        g_behavior.on_weapon_attack("sword", (float)i * 2.0f + 1.5f, 1,
                                    150.0f, 100.0f, 1, 1);
    }
    PlayerHabitProfile prof = PlayerBehaviorAnalyzer::analyze(g_behavior.history());
    EXPECT_GT(prof.attack_frequency, 0.0f);
    (void)prof;
}

// 二分定位: 战术链在完整流上构建是否崩溃
}  // namespace
