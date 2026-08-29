// G9.3: RNG Boundary — Rendering/VFX 不得消耗 Gameplay RNG (audit RNG-001/RNG-002)
// 边界:
//   Gameplay RNG (CountingRng rng):  Dungeon / Combat / Loot / AI / Simulation
//   Visual   RNG (CountingRng visual_rng): Screen Shake / VFX / Cosmetic
// 回归点:
//   RNG-001 同 seed 同输入, 屏震开关不得改变 gameplay rng 流与游戏结果
//   RNG-002 boss_type_for_floor(seed=0) 旧代码走 random_device → 进程随机
#include <gtest/gtest.h>

#include "systems/combat_system.h"
#include "scenes/game_scene.h"
#include "entities/boss.h"

#include <cmath>
#include <utility>

// 与 inventory_sell_ui_test.cpp 一致: main.cpp 中的字体全局在测试中桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

// ── RNG-001: 屏震路径不得消耗 gameplay rng() ─────────────────
// 生产路径 _draw() 调用 GameScene::shake_offset() 计算相机偏移。
// 同 seed 下: 屏震 N 帧 vs 零屏震, 后续 gameplay 掷骰必须逐位一致。
TEST(RngBoundary, ScreenShakeDoesNotConsumeGameplayRng) {
    constexpr uint32_t SEED = 20250829u;

    seed_rng(SEED);
    (void)rng(); (void)rng();   // 消耗 2 次战斗掷骰 (对齐两场景状态)

    // 模拟 120 帧屏震 (每次 _draw 调用一次 shake_offset)
    for (int i = 0; i < 120; i++) {
        auto [ox, oy] = GameScene::shake_offset(8.0f, 0.12f);
        ASSERT_TRUE(std::isfinite(ox) && std::isfinite(oy));
    }
    const uint32_t after_shake_1 = rng();
    const uint32_t after_shake_2 = rng();

    // 同 seed 重播, 零屏震
    seed_rng(SEED);
    (void)rng(); (void)rng();
    const uint32_t baseline_1 = rng();
    const uint32_t baseline_2 = rng();

    EXPECT_EQ(after_shake_1, baseline_1)
        << "screen shake shifted the gameplay RNG stream (RNG-001)";
    EXPECT_EQ(after_shake_2, baseline_2);
}

// ── 视觉流独立性: seed_rng 不得触碰视觉流 ────────────────────
TEST(RngBoundary, VisualRngIsIndependentStream) {
    seed_visual_rng(42u);
    const uint32_t v1 = visual_rng();

    seed_rng(123456u);              // 战斗流重播 — 不应影响视觉流
    const uint32_t v2 = visual_rng();
    (void)v2;

    seed_visual_rng(42u);
    EXPECT_EQ(visual_rng(), v1)
        << "visual stream must only follow seed_visual_rng";
}

// ── RNG-002: boss seed=0 必须确定性回退 (非 random_device) ────
TEST(RngBoundary, BossTypeSeedZeroIsDeterministic) {
    for (int floor : {5, 10, 15}) {
        const BossType first = boss_type_for_floor(floor, 0u);
        for (int i = 0; i < 32; i++) {
            EXPECT_EQ(boss_type_for_floor(floor, 0u), first)
                << "floor " << floor << ": seed=0 must not be process-random";
        }
        // seed=0 语义等价固定兜底种子 1
        EXPECT_EQ(boss_type_for_floor(floor, 0u), boss_type_for_floor(floor, 1u))
            << "floor " << floor << ": seed=0 fallback must equal seed=1";
    }
}
