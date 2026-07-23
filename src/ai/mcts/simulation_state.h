#pragma once
// G8.3: SimulationState — minimal combat snapshot for MCTS rollouts.
// Clones only combat-relevant fields: HP, position, attack, defense, buffs, cooldowns.
// Zero dependency on GameScene, Renderer, Audio, or Presentation.

#include <vector>
#include <string>
#include <cstdint>

namespace mcts {

// ── Player snapshot ────────────────────────────────────────
struct BuffSnapshot {
    std::string id;
    int stacks = 0;
    float remaining = 0;
};

struct PlayerSnapshot {
    float hp = 100, max_hp = 100;
    float x = 0, y = 0;
    int attack = 10;
    int pdef = 3, mdef = 1;
    std::vector<BuffSnapshot> buffs;
    float attack_cooldown = 0;
    float skill_cooldowns[4] = {0, 0, 0, 0};
    bool alive = true;
};

// ── Monster snapshot ───────────────────────────────────────
struct MonsterSnapshot {
    std::string type;
    float hp = 20, max_hp = 20;
    float x = 0, y = 0;
    int attack = 4;
    int pdef = 1, mdef = 0;
    std::vector<BuffSnapshot> buffs;
    bool alive = true;
    bool is_boss = false;
};

// ── RNG snapshot (for deterministic rollouts) ──────────────
struct RNGState {
    uint32_t seed = 0;
    uint32_t calls = 0;
    uint32_t next() const;  // deterministic: seed + calls
};

// ── Full simulation state ──────────────────────────────────
struct SimulationState {
    PlayerSnapshot player;
    std::vector<MonsterSnapshot> monsters;
    RNGState rng;
    int depth = 0;
    bool terminal = false;
    bool victory = false;      // all monsters dead

    SimulationState clone() const;

    bool is_terminal() const { return terminal || !player.alive; }
    int alive_monsters() const;
};

} // namespace mcts
