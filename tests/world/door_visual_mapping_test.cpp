// G10.2-B2: Door Visual Asset Mapping — 门视觉资产四态映射回归
// 与 door_truth_table_test (Gameplay 语义) 分层:
//   TruthTable = 门游戏逻辑 (可走性/视线/E键/R1)
//   本测试     = 门视觉资产 (DoorState → Asset ID + overlay 渲染规则)
// 未来改美术/换贴图只影响本层, 不误伤 Truth Table。
#include <gtest/gtest.h>

#include "rendering/door_renderer.h"
#include "resources/resource_manager.h"

#include <string>

// 与既有测试一致: main.cpp 的字体全局桩化 (DoorRenderer 经 link 引用)
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

// ── 映射核心: OPEN→door.open, CLOSED→door.closed, LOCKED→door.locked, SEALED→door.sealed ──
TEST(DoorVisualMapping, StateToAssetId) {
    EXPECT_STREQ(DoorRenderer::asset_id_for(DoorState::OPEN),   "door.open");
    EXPECT_STREQ(DoorRenderer::asset_id_for(DoorState::CLOSED), "door.closed");
    EXPECT_STREQ(DoorRenderer::asset_id_for(DoorState::LOCKED), "door.locked");
    EXPECT_STREQ(DoorRenderer::asset_id_for(DoorState::SEALED), "door.sealed");
}

// ── LOCKED 必须可视觉区分 (D3): overlay 非透明, 其他态透明 ──
TEST(DoorVisualMapping, LockedOverlayDistinct) {
    const Color locked = DoorRenderer::overlay_for(DoorState::LOCKED);
    EXPECT_GT(locked.a, 0u) << "LOCKED 必须有色罩 (视觉区分于 CLOSED)";
    // #B03030 → 红通道显著高于 0
    EXPECT_GT(locked.r, 100u) << "LOCKED overlay 应近似 #B03030 (r=" << (int)locked.r << ")";
    for (DoorState s : {DoorState::OPEN, DoorState::CLOSED, DoorState::SEALED}) {
        const Color c = DoorRenderer::overlay_for(s);
        EXPECT_EQ(c.a, 0u) << "非 LOCKED 态不得有色罩";
    }
}

// ── manifest 一致性: 映射的 ID 必须已装载, locked 带 overlay 规则 ──
TEST(DoorVisualMapping, ManifestAgreement) {
    auto& rm = ResourceManager::inst();
    ASSERT_TRUE(rm.load_assets_config());
    const AssetDef* locked = rm.asset_by_id("door.locked");
    ASSERT_NE(locked, nullptr);
    EXPECT_FALSE(locked->overlay.empty()) << "door.locked overlay 规则缺失";
    EXPECT_GT(locked->overlay_alpha, 0.0f);
    for (const char* id : {"door.open", "door.closed", "door.sealed"}) {
        const AssetDef* d = rm.asset_by_id(id);
        ASSERT_NE(d, nullptr) << id << " 未装载";
        EXPECT_TRUE(d->overlay.empty()) << id << " 不应有 overlay";
    }
}