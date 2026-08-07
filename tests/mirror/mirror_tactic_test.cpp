// M4.1 验证回归 — 三场景决策触发 (2026-08-07)
// 1. SNIPER 玩家 → 远程压制 (OPEN_RANGED/KITE)
// 2. 时停玩家 → 压进冻结+突进前置 (ENGAGE_MELEE)
// 3. 低血红血回血玩家 → 压迫 (ENGAGE_MELEE)
#include <gtest/gtest.h>
#include "game/director/mirror_combat_director.h"
#include "ai/player_behavior/player_habit_profile.h"

namespace {

using T = MirrorTactic;

PlayerHabitProfile SniperProfile() {
    PlayerHabitProfile p;
    p.style = PlayerStyle::SNIPER;
    p.average_distance = 300.0f;   // 偏好远距离 (>260)
    p.aggression_score = 0.3f;
    return p;
}

PlayerHabitProfile TimeStopPlayerProfile() {
    PlayerHabitProfile p;
    // 技能重度使用者, 主力 the_world (sk3) → MAGE + skill spam
    p.style = PlayerStyle::MAGE;
    p.skill_frequency = 0.6f;
    p.skill_preference[3] = 0.8f;
    p.predict_skill_spam = true;
    p.aggression_score = 0.7f;   // 常放技能迎战 → 压迫近身
    return p;
}

PlayerHabitProfile LowHpHealerProfile() {
    PlayerHabitProfile p;
    p.style = PlayerStyle::BALANCED;
    p.heal_frequency = 0.05f;
    p.predict_panic_heal = true;
    return p;
}

// ── 场景 1: SNIPER 玩家 → 远程压制 ──
TEST(M41TacticVerification, SniperTriggersRangedSuppression) {
    auto p = SniperProfile();
    EXPECT_EQ(MirrorCombatDirector::decide_tactic(p, 6.0f * 32.0f, 0.9f),
              T::OPEN_RANGED);
}

TEST(M41TacticVerification, SniperAtCloseRangeKites) {
    auto p = SniperProfile();
    EXPECT_EQ(MirrorCombatDirector::decide_tactic(p, 2.0f * 32.0f, 0.9f),
              T::KITE);
}

TEST(M41TacticVerification, SniperVariantsTrigger) {
    // average_distance 维度单独触发 (非 SNIPER style 也可)
    PlayerHabitProfile far;
    far.average_distance = 270.0f;
    far.style = PlayerStyle::BALANCED;
    EXPECT_EQ(MirrorCombatDirector::decide_tactic(far, 6.0f * 32.0f, 0.9f),
              T::OPEN_RANGED);
}

// ── 场景 2: 时停玩家 → 压进 (配合 case4 冻结 + 突进) ──
TEST(M1TacticVerification, TimeStopPlayerAgressivePushesIn) {
    auto p = TimeStopPlayerProfile();
    // 高 aggression + skill spam → ENGAGE (技能时刻打断 + 贴脸)
    EXPECT_EQ(MirrorCombatDirector::decide_tactic(p, 6.0f * 32.0f, 0.9f),
              T::ENGAGE_MELEE);
}

// ── 场景 3: 低血红血玩家 → 压迫 ──
TEST(M1TacticVerification, LowHpForcesEngagement) {
    auto healer = LowHpHealerProfile();
    EXPECT_EQ(MirrorCombatDirector::decide_tactic(healer, 6.0f * 32.0f, 0.25f),
              T::ENGAGE_MELEE);   // 27% 血 → 压进终结, 抑制其回血
}

TEST(M1TacticVerification, HealthyPlayerNotForced) {
    auto healer = LowHpHealerProfile();
    EXPECT_NE(MirrorCombatDirector::decide_tactic(healer, 6.0f * 32.0f, 0.9f),
              T::ENGAGE_MELEE);   // 高血时不由低血分支驱动
}

}  // namespace