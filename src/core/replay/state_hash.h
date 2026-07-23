#pragma once
#include <cstdint>
#include <vector>
#include <memory>

// Forward declare — full definition included in .cpp
class Player;
class Monster;

// ============================================================
// G4.5: StateHash — running chain hash for replay verification
// hash = mix(previous_hash, player_state, floor, monster_state)
// ============================================================

uint64_t compute_state_hash(uint64_t prev_hash,
    const Player& player, int floor,
    const std::vector<std::unique_ptr<Monster>>& monsters);

// ── verification helper ──
bool verify_hash_chain(const std::vector<uint64_t>& expected,
                       const std::vector<uint64_t>& actual);
