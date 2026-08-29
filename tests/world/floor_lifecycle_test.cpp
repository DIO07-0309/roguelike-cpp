// G9.2: Floor transition lifecycle regression — audit LIFE-001/002/003
// 背景: PROJECT_TECHNICAL_AUDIT.md §L3 三项 P1 (enter_floor 重置不完整)
//   LIFE-001 Challenge 相位跨层残留 → 免钥匙挑战/幽灵 COMBAT
//   LIFE-002 上一层 room rect/door group 在 Boss 层继续 tick → 卡 ARMED/错误封门
//   LIFE-003 exit_challenge_arena 调 reset_visibility → 吞掉本层探索进度
// 驱动真实 GameScene::enter_floor / enter|exit_challenge_arena 调用侧路径
// (headless 安全: DoorRenderer::init 已有 IsWindowReady 守卫)
#include <gtest/gtest.h>

#include "scenes/game_scene.h"
#include "world/challenge_room.h"
#include "world/room_manager.h"
#include "world/game_map.h"
#include "player.h"
#include "data/boss_defs.h"
#include "data/enemy_defs.h"

#include <memory>

// 与 inventory_sell_ui_test.cpp 一致: main.cpp 中的字体全局在测试中桩化
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

static void load_registry_defs() {
    load_boss_defs("resources/bosses.json");
    load_enemy_defs("resources/enemies.json");
}

static std::unique_ptr<GameScene> make_scene() {
    load_registry_defs();
    auto s = std::make_unique<GameScene>();
    s->player = std::make_unique<Player>(0, 0, 200, 200, 10, 5, 3);
    return s;
}

// ── LIFE-001: Challenge 相位禁止跨层残留 ─────────────────────
// 最危险残留: UNLOCKED (钥匙已消耗未进入) → 新层免钥匙挑战;
// COMBAT 残留 → is_save_blocked() 在新层幽灵拦截。
TEST(FloorLifecycle, ChallengePhaseDoesNotSurviveFloorTransition) {
    const ChallengePhase stale_phases[] = {
        ChallengePhase::UNLOCKED, ChallengePhase::CLEARED,
        ChallengePhase::COMBAT,   ChallengePhase::ARMED,
    };
    for (auto stale : stale_phases) {
        auto s = make_scene();
        s->enter_floor(1, 4242u);
        // 模拟玩家在挑战流程中途下楼 → 相位悬空跨层。
        // 用超出地图边界的哨兵值 — 无论新层是否恰好有挑战房, 残留均可暴露。
        s->challenge_ctrl().set_phase_for_test(stale);
        s->challenge_ctrl().set_room_rect(900, 900, 777, 666);
        s->challenge_ctrl().set_return_portal(900, 900);

        s->enter_floor(2, 4243u);

        const auto ph = s->challenge_ctrl().phase();
        EXPECT_TRUE(ph == ChallengePhase::INACTIVE ||
                    ph == ChallengePhase::PORTAL_ACTIVE)
            << "stale challenge phase survived floor transition";
        // 上一层房间矩形/返还传送门坐标在任何相位下都不得存活
        EXPECT_NE(s->challenge_ctrl().room_rw(), 777);
        EXPECT_NE(s->challenge_ctrl().room_rh(), 666);
        EXPECT_NE(s->challenge_ctrl().room_rx(), 900);
        EXPECT_NE(s->challenge_ctrl().return_portal_tx(), 900);
    }
}

// ── LIFE-002: 上一层 RoomManager 数据禁止带入 Boss 层 ────────
TEST(FloorLifecycle, RoomManagerNotReusedOnBossFloor) {
    auto s = make_scene();
    s->enter_floor(1, 99u);
    ASSERT_GT(s->room_mgr().room_count(), 0) << "normal floor must build rooms";

    s->enter_floor(5, 99u);   // Boss 层: 不走 build 分支
    EXPECT_EQ(s->state, GameState::BOSS_INTRO);
    ASSERT_EQ(s->room_mgr().room_count(), 0)
        << "previous floor room rects must not survive into boss floor";

    // Boss 层 tick 不得 ARM/封门 (模拟 intro 结束后的正常更新)
    s->state = GameState::PLAYING;
    s->room_mgr().tick(s->game_map.get(), s->player.get(), s->monsters);
    ASSERT_EQ(s->room_mgr().room_count(), 0);

    int locked_doors = 0;
    for (int y = 0; y < s->game_map->height; y++)
        for (int x = 0; x < s->game_map->width; x++)
            if (s->game_map->door_state_at(x, y) == DoorState::LOCKED)
                locked_doors++;
    EXPECT_EQ(locked_doors, 0)
        << "stale door-group logic must not lock doors on boss floor";
}

// ── LIFE-003: 挑战竞技场往返不得清空本层探索进度 ──────────────
// 旧缺陷: exit_challenge_arena 调 reset_visibility 清空 is_explored,
// 返还后仅玩家周围一圈被 update_fov 重建 — 远处已探索区被吞。
TEST(FloorLifecycle, ArenaRoundTripPreservesExploration) {
    auto s = make_scene();
    s->enter_floor(1, 777u);

    // 在玩家出生点做一次 FOV → 区域 A
    auto [px, py] = s->game_map->pixel_to_tile(
        s->player->entity.position.x, s->player->entity.position.y);
    s->game_map->update_fov(px, py, 8);
    ASSERT_TRUE(s->game_map->isExplored(px, py));

    // 找一个远离出生点的可通行格, 传送过去再做 FOV → 区域 B
    int qx = -1, qy = -1;
    for (int y = 0; y < s->game_map->height && qx < 0; y++)
        for (int x = 0; x < s->game_map->width; x++) {
            if (s->game_map->is_walkable(x, y) &&
                std::abs(x - px) + std::abs(y - py) > 25) {
                qx = x; qy = y; break;
            }
        }
    ASSERT_GE(qx, 0) << "dungeon map too small for far-tile probe";
    auto [qxp, qyp] = s->game_map->tile_to_pixel(qx, qy);
    s->player->entity.position = {(float)qxp, (float)qyp};
    s->player->entity.sync_rect();
    s->game_map->update_fov(qx, qy, 8);
    // 区域 A 不在区域 B 的 FOV 内, 但仍应处于已探索状态
    ASSERT_TRUE(s->game_map->isExplored(px, py));

    // 真实路径: 进出挑战竞技场
    s->enter_challenge_arena();
    ASSERT_EQ(s->world_mode(), WorldMode::CHALLENGE_ARENA);
    s->exit_challenge_arena();
    ASSERT_EQ(s->world_mode(), WorldMode::DUNGEON);

    // 回归点: 返还后区域 A 不得被吞 (旧代码此处 is_explored == false)
    EXPECT_TRUE(s->game_map->isExplored(px, py))
        << "arena round trip must not wipe this floor's exploration";
    EXPECT_TRUE(s->game_map->isExplored(qx, qy));
}
