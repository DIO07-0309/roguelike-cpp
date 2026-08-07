// M4.4: TacticalChainTable — 战术链 n-gram 序列记忆
#include <gtest/gtest.h>
#include "ai/mirror/tactical_chain_table.h"
#include "ai/mirror/mirror_agent.h"
#include "ai/player_behavior/player_action.h"

namespace {

PlayerAction MakeAction(PlayerActionType t, int value, int skill_id,
                        int weapon_type, int combo_stage) {
    PlayerAction a;
    a.type = t; a.value = value; a.skill_id = skill_id;
    a.weapon_type = weapon_type; a.combo_stage = combo_stage;
    a.timestamp = 0.0f;
    return a;
}

TEST(TacticalChain, SymbolMappingSkill) {
    auto a = MakeAction(PlayerActionType::SKILL, 0, 2, -1, -1);
    EXPECT_EQ(symbol_from_action(a), (int)TacticalSymbol::SKILL_2);
}

TEST(TacticalChain, SymbolMappingMove) {
    auto a = MakeAction(PlayerActionType::MOVE, 1, -1, -1, -1);
    EXPECT_EQ(symbol_from_action(a), (int)TacticalSymbol::MOVE_R);
}

TEST(TacticalChain, SymbolMappingCombo) {
    auto a = MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 1);
    EXPECT_EQ(symbol_from_action(a), (int)TacticalSymbol::COMBO_2);
}

TEST(TacticalChain, SymbolMappingComboStart) {
    auto a = MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 2);
    EXPECT_EQ(symbol_from_action(a), (int)TacticalSymbol::COMBO_3);
}

TEST(TacticalChain, BuildCountsTrigram) {
    TacticalChainTable t;
    std::vector<PlayerAction> h;
    // 连招1 → 连招2 交替 4 次 → (COMBO_1,COMBO_2) 前缀出现 ≥3 次, 后继恒 COMBO_1
    for (int i = 0; i < 4; i++) {
        h.push_back(MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 0));
        h.push_back(MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 1));
    }
    t.build(h);
    // (COMBO_1, COMBO_2) 前缀样本 3 次 → 预测后继 COMBO_1 (3-gram)
    auto p = t.predict((int)TacticalSymbol::COMBO_1,
                       (int)TacticalSymbol::COMBO_2);
    EXPECT_EQ(p.level, 0);
    EXPECT_EQ(p.best, (int)TacticalSymbol::COMBO_1);
}

TEST(TacticalChain, DegradeTo2Gram) {
    TacticalChainTable t;
    std::vector<PlayerAction> h;
    // 右侧移 + 连招1 交替 6 次: s0=MOVE_R 出现 5 次(≥5) 后总是 COMBO_1 → 2-gram 学到
    for (int i = 0; i < 6; i++) {
        h.push_back(MakeAction(PlayerActionType::MOVE, 1, -1, -1, -1));
        h.push_back(MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 0));
    }
    t.build(h);
    // 无 (SKILL_3, MOVE_R) 3-gram → predict 内自动降级到 fuzzy2(MOVE_R)
    auto p = t.predict((int)TacticalSymbol::SKILL_3,
                       (int)TacticalSymbol::MOVE_R);
    EXPECT_EQ(p.level, 1);   // 2-gram
    EXPECT_EQ(p.best, (int)TacticalSymbol::COMBO_1);
}

TEST(TacticalChain, EmptyStreamSafe) {
    TacticalChainTable t;
    std::vector<PlayerAction> h;
    t.build(h);
    EXPECT_EQ(t.entries(), 0);
}

// M4.4: 战术链仲裁 — 高置信 3-gram 驱动应对臂
// 在线只提供 PlayerActionType → 类型级近似符号 (SKILL→SKILL_0, ATTACK→COMBO_0=COMBO_1)
TEST(TacticalChainArbitration, ChainLayerDrivesArm) {
    PlayerHabitProfile prof;
    MirrorAgent agent;
    agent.init(prof);
    agent.set_phase(2);
    auto chain = std::make_unique<TacticalChainTable>();
    // 玩家固定模式: 技能0 → 连招1 → 连招2 (重复 4 次) → (SKILL_0,COMBO_1)↗COMBO_2 高置信
    std::vector<PlayerAction> h;
    for (int i = 0; i < 4; i++) {
        h.push_back(MakeAction(PlayerActionType::SKILL, 0, 0, -1, -1));
        h.push_back(MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 0));
        h.push_back(MakeAction(PlayerActionType::ATTACK, 0, -1, 2, 1));
    }
    chain->build(h);
    agent.set_chain_table(std::move(chain));
    // 在线观察 技能 → 普攻 → buffer (SKILL_0, COMBO_1)
    agent.observe_actual(PlayerActionType::SKILL);
    agent.observe_actual(PlayerActionType::ATTACK);
    int arm = agent.recommend_action(MirrorBattleState{});
    EXPECT_EQ(arm, (int)MirrorAction::COMBO);   // 预测连招2 → ATTACK 意图 → 连招应对臂
}

}  // namespace