#include <gtest/gtest.h>
#include "challenge_room.h"
#include "player.h"
#include "game_map.h"

static Player make_player(int keys = 3) {
    Player p(0, 0, 200, 200, 10, 5, 3);
    p.key_count = keys;
    p.gold = 100;
    return p;
}

// ============================================================
// Batch 3I: Challenge Portal — Unit Tests
// ============================================================

// --- Key Consumption ---

TEST(ChallengePortal, ConsumeKeySuccess) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(1);
    EXPECT_TRUE(c.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengePortal, ConsumeKeyFailNoKey) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(0);
    EXPECT_FALSE(c.consume_key_for_challenge(p));
}

TEST(ChallengePortal, ConsumeKeyFailWrongPhase) {
    ChallengeRoomController c;
    Player p = make_player(1);
    EXPECT_FALSE(c.consume_key_for_challenge(p));
}

TEST(ChallengePortal, ConsumeKeyDoesNotAdvancePhase) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(1);
    c.consume_key_for_challenge(p);
    EXPECT_EQ(c.phase(), ChallengePhase::PORTAL_ACTIVE);
}

// --- World Switch ---

TEST(ChallengePortal, SetupPortalSetsPhase) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    EXPECT_EQ(c.phase(), ChallengePhase::PORTAL_ACTIVE);
}

TEST(ChallengePortal, PortalCoordinatesStored) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    EXPECT_EQ(c.portal_tx(), 10);
    EXPECT_EQ(c.portal_ty(), 5);
}

TEST(ChallengePortal, RoomRectStored) {
    ChallengeRoomController c;
    c.set_room_rect(5, 5, 8, 6);
    EXPECT_EQ(c.room_rx(), 5);
    EXPECT_EQ(c.room_ry(), 5);
    EXPECT_EQ(c.room_rw(), 8);
    EXPECT_EQ(c.room_rh(), 6);
}

// --- Return Portal ---

TEST(ChallengePortal, ReturnPortalSetAfterClear) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.set_room_rect(5, 5, 8, 6);
    c.set_return_portal(8, 12);
    EXPECT_EQ(c.return_portal_tx(), 8);
    EXPECT_EQ(c.return_portal_ty(), 12);
}

TEST(ChallengePortal, MarkClearedSetsPhase) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.mark_cleared();
    EXPECT_EQ(c.phase(), ChallengePhase::CLEARED);
    EXPECT_TRUE(c.is_cleared());
}

// --- State Preservation ---

TEST(ChallengePortal, ResetClearsAllState) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.set_return_portal(8, 12);
    c.set_room_rect(5, 5, 8, 6);
    c.reset();
    EXPECT_EQ(c.portal_tx(), -1);
    EXPECT_EQ(c.portal_ty(), -1);
    EXPECT_EQ(c.return_portal_tx(), -1);
    EXPECT_EQ(c.return_portal_ty(), -1);
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

TEST(ChallengePortal, ResetDoesNotAffectPlayer) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(2);
    c.consume_key_for_challenge(p);
    c.reset();
    EXPECT_EQ(p.key_count, 1);
    EXPECT_EQ(p.combat.current_hp, 200);
}

TEST(ChallengePortal, ClearThenResetPreservesKey) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.setup_portal(10, 5);
    c.consume_key_for_challenge(p);
    c.mark_cleared();
    c.reset();
    EXPECT_EQ(p.key_count, 2);
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

// --- Save Block ---

TEST(ChallengePortal, IsSaveBlockedDuringCombat) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(1);
    c.consume_key_for_challenge(p);

    c.set_phase_for_test(ChallengePhase::ARMED);
    EXPECT_TRUE(c.phase() == ChallengePhase::ARMED);

    c.set_phase_for_test(ChallengePhase::WAVE_SPAWNING);
    EXPECT_TRUE(c.phase() == ChallengePhase::WAVE_SPAWNING);

    c.set_phase_for_test(ChallengePhase::COMBAT);
    EXPECT_TRUE(c.phase() == ChallengePhase::COMBAT);

    c.set_phase_for_test(ChallengePhase::WAIT_NEXT_WAVE);
    EXPECT_TRUE(c.phase() == ChallengePhase::WAIT_NEXT_WAVE);
}

TEST(ChallengePortal, SaveAllowedWhenInactive) {
    ChallengeRoomController c;
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

TEST(ChallengePortal, SaveAllowedWhenCleared) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.mark_cleared();
    EXPECT_TRUE(c.is_cleared());
}

// --- Duplicate Reward Prevention ---

TEST(ChallengePortal, MarkClearedIdempotent) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.mark_cleared();
    EXPECT_TRUE(c.is_cleared());
    c.mark_cleared();
    EXPECT_TRUE(c.is_cleared());
}

TEST(ChallengePortal, ConsumeKeyAfterClearedFails) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    c.mark_cleared();
    Player p = make_player(1);
    EXPECT_FALSE(c.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 1);
}

TEST(ChallengePortal, TryActivateAfterPortalFails) {
    ChallengeRoomController c;
    c.setup_portal(10, 5);
    Player p = make_player(1);
    EXPECT_FALSE(c.try_activate(p));
    EXPECT_EQ(p.key_count, 1);
}

// --- Full Flow ---

TEST(ChallengePortal, FullFlow) {
    ChallengeRoomController c;
    Player p = make_player(2);

    c.setup_portal(10, 5);
    EXPECT_EQ(c.phase(), ChallengePhase::PORTAL_ACTIVE);

    EXPECT_TRUE(c.consume_key_for_challenge(p));
    EXPECT_EQ(p.key_count, 1);

    c.mark_cleared();
    EXPECT_TRUE(c.is_cleared());

    c.set_return_portal(8, 12);
    EXPECT_GT(c.return_portal_tx(), 0);

    c.reset();
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}
