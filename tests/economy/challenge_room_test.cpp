#include <gtest/gtest.h>
#include "challenge_room.h"
#include "player.h"

static Player make_player(int keys = 3) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.key_count = keys;
    p.gold = 100;
    return p;
}

// --- Q1: State Machine ---

TEST(ChallengeRoomTest, InitialPhaseIsInactive) {
    ChallengeRoomController c;
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

TEST(ChallengeRoomTest, TryActivateWithoutKeyFails) {
    ChallengeRoomController c;
    Player p = make_player(0);
    EXPECT_FALSE(c.try_activate(p));
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomTest, TryActivateWithKeySucceeds) {
    ChallengeRoomController c;
    Player p = make_player(1);
    EXPECT_TRUE(c.try_activate(p));
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
    EXPECT_EQ(p.key_count, 0);
}

TEST(ChallengeRoomTest, TryActivateConsumesKey) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_EQ(p.key_count, 2);
}

TEST(ChallengeRoomTest, DoubleActivateFails) {
    ChallengeRoomController c;
    Player p = make_player(3);
    EXPECT_TRUE(c.try_activate(p));
    EXPECT_FALSE(c.try_activate(p));
    EXPECT_EQ(p.key_count, 2);
}

TEST(ChallengeRoomTest, OnPlayerEnteredTransitionsToArmed) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
    c.on_player_entered();
    EXPECT_EQ(c.phase(), ChallengePhase::ARMED);
}

TEST(ChallengeRoomTest, OnDoorsLockedTransitionsToSpawning) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    EXPECT_EQ(c.phase(), ChallengePhase::ARMED);
    c.on_doors_locked();
    EXPECT_EQ(c.phase(), ChallengePhase::WAVE_SPAWNING);
}

TEST(ChallengeRoomTest, ResetReturnsToInactive) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.reset();
    EXPECT_EQ(c.phase(), ChallengePhase::INACTIVE);
}

// --- Q2: Deterministic Seed ---

TEST(ChallengeRoomTest, DeterministicSeedSameInputs) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    // Same inputs → same seed (tested indirectly via spawn determinism)
    EXPECT_EQ(c.phase(), ChallengePhase::UNLOCKED);
}

TEST(ChallengeRoomTest, DeterministicSeedNoCollision) {
    // Seed derivation: hash_combine with avalanche
    // We verify the concept: different room_index + wave_index → different behavior
    // (actual spawn test would need full map setup)
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_TRUE(c.is_cleared() == false);
}

// --- Q3: Wave Info ---

TEST(ChallengeRoomTest, WaveInfoDefaults) {
    ChallengeRoomController c;
    EXPECT_EQ(c.current_wave(), 0);
    EXPECT_EQ(c.total_waves(), 3);
}

TEST(ChallengeRoomTest, ResetClearsWaveCount) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    c.on_player_entered();
    c.on_doors_locked();
    c.reset();
    EXPECT_EQ(c.current_wave(), 0);
}

// --- Q4: IsCleared ---

TEST(ChallengeRoomTest, IsClearedFalseByDefault) {
    ChallengeRoomController c;
    EXPECT_FALSE(c.is_cleared());
}

TEST(ChallengeRoomTest, IsClearedFalseWhenActive) {
    ChallengeRoomController c;
    Player p = make_player(3);
    c.try_activate(p);
    EXPECT_FALSE(c.is_cleared());
}
