# Batch 3D — Challenge Room Architecture Audit

> **Status:** Design Phase — Awaiting Review Gate
> **Date:** 2026-08-28
> **Scope:** Architecture audit + minimal implementation plan for Challenge Room

---

## Part 1: Current Architecture Audit

### A. SpecialRoom System

**Enum (10 values):** `ALTAR, TREASURE, FOUNTAIN, SHOP, BLACKSMITH, LIBRARY, GAMBLER, SHRINE, SECRET, LANDMARK`

**Placement algorithm** (`dungeon_generator.cpp:89-143`):
1. Room 0 (spawn) and `_rooms.back()` (exit) are excluded
2. Fisher-Yates shuffle of candidate rooms (room 1 pinned near spawn)
3. Types assigned sequentially via `special_room_from_index(type_idx++)`
4. ~50% chance to replace with LANDMARK if biome has landmarks
5. 30% chance to convert one room to SECRET

**BSP adjacency: NO adjacency graph exists.** Connectivity is implicit in tree topology. No API for "rooms near room X."

**Floor config:** 0-5 special rooms per floor (0 on boss floors, 3-5 on normal).

### B. Key System

| Component | Status |
|-----------|--------|
| `Player::key_count` | ✅ Exists (`player.h:55`) |
| `add_key()` / `spend_key()` / `get_key_count()` | ✅ Implemented (`player.cpp:332-343`) |
| HUD display | ✅ `K: %d` (`game_renderer.cpp:779`) |
| Save v4 persistence | ✅ `key:<int>` line |
| **`spend_key()` production callers** | **❌ ZERO — dead code** |

**Keys are accumulated (Gambler room 20% roll) but never spent.** Challenge Room would be the first consumer.

### C. Exit Room & BSP Placement

**Exit room = `_rooms.back()`** (last BSP room). Stairs tile placed at center, activated when all monsters die.

**No adjacency data.** To find "rooms near exit," we must use distance heuristics (Euclidean distance between room centers from `get_room_centers()`).

**Candidate strategy for Challenge Room:**
1. Compute distance from each candidate room center to exit room center
2. Filter rooms with distance < threshold (e.g., within 40% of max distance)
3. Pick the closest non-special, non-spawn, non-exit room
4. Fallback: if no room qualifies, skip Challenge Room for this floor

### D. Room Encounter System

**State machine:** `IDLE → ARMED → LOCKED → CLEARED`

| State | Meaning |
|-------|---------|
| IDLE | No monsters / cleared |
| ARMED | Player entered, alive monsters detected |
| LOCKED | Doors sealed, combat active |
| CLEARED | All monsters dead, doors reopened |

**Critical finding: RoomManager does NOT spawn monsters.** It only locks/unlocks doors around **pre-existing** monsters placed by `FloorManager::spawn_floor_monsters`.

**For Challenge Room, we need a different approach:** Pre-placed monsters won't work because Challenge Room should spawn waves AFTER the door is locked. We need a small wave spawner.

### E. Monster Generation & Elite System

| System | File | API |
|--------|------|-----|
| Monster factory | `monster.cpp:320` | `spawn_monster(px, py, type)` — data-driven from `enemies.json` |
| Floor scaling | `growth_curve.cpp:10-42` | `g_growth.hp_scale(floor)`, `g_growth.atk_scale(floor)` |
| Elite scale | `growth_curve.cpp:30` | `g_growth.elite_scale(floor)` — 2.0→3.5 over 15 floors |
| Elite types | `enemies.json` | `elite_slime`, `elite_orc`, `necromancer`, `storm_elemental`, `ice_warden` |
| Elite buffs | `enemies.json` | Random from `elite_buff_pool` (attack_up, shield, etc.) |
| Monster cap | `floor_config.cpp` | Per-floor count (4-8), but **no runtime cap** |
| Spawn positions | `floor_manager.cpp:53` | Random offset from room center, `is_walkable()` check only |

**No player-overlap or monster-overlap check on spawn.**

### F. Reward System

| System | Status |
|--------|--------|
| `RewardManager` | ✅ 4 methods: `grant_item`, `grant_gold`, `grant_key`, `grant_relic` |
| `generate_random_item()` | ✅ Data-driven, `random_rarity()` with fixed weights |
| Rarity weights | `items.json`: COMMON=60, RARE=25, EPIC=12, LEGENDARY=3 |
| Rarity biasing | Shop room retries up to 5× if rarity < RARE |
| Gold sources | **Only selling items** — no monster gold drops |
| Boss rewards | Direct `push_back`, bypass RewardManager |

---

## Part 2: Challenge Room Minimal Architecture

### Core Loop

```
Player explores floor
    ↓
Finds Challenge Room near exit
    ↓
Door is LOCKED (visual: red lock)
    ↓
[E] → "需要钥匙开门" (if no key)
[E] → spend_key(1) → door OPEN → ARMED
    ↓
Wave 1 spawns (3-5 monsters)
    ↓
Room LOCKED (doors seal)
    ↓
Wave 1 cleared → 2s delay → Wave 2 spawns
    ↓
Wave 2 cleared → 3s delay → Wave 3 (final)
    ↓
All waves cleared → CLEARED → doors OPEN
    ↓
High-quality reward drops
    ↓
Player continues to exit
```

### Q1: New SpecialRoomType?

**Decision: Add `CHALLENGE` to enum.** (11th value)

Rationale:
- Challenge Room has unique semantics (key door + wave spawning + delayed rewards)
- Cannot reuse existing type without overloading semantics
- 1 line added to enum, 1 case added to switch statements
- Minimal blast radius

### Q2: Placement Near Exit?

**Decision: Distance heuristic, no BSP rewrite.**

```cpp
// In _assign_special_rooms, after existing special room placement:
// Reserve one slot for CHALLENGE on floors ≥ 3 (non-boss)
if (floor >= 3 && !is_boss_floor) {
    // Find room closest to exit among unassigned rooms
    auto exit_center = rooms.back();
    int best = -1; float best_dist = FLT_MAX;
    for (int i = 1; i < rooms.size() - 1; i++) {
        if (room_has_special(i)) continue;
        float d = distance(rooms[i], exit_center);
        if (d < best_dist) { best_dist = d; best = i; }
    }
    if (best >= 0) assign_special(best, SpecialRoomType::CHALLENGE);
}
```

**Result:** Challenge Room appears in a room near (but not at) the exit. Always optional — player can walk past it.

### Q3: Door Initial State

**Decision: `CLOSED` (not LOCKED).**

Flow:
1. Room generated with `CLOSED` doors
2. Player approaches, sees `[E]` interaction prompt
3. Press E:
   - Has key → `spend_key(1)` → doors OPEN → wave spawning starts
   - No key → message "需要钥匙开门"
4. After doors OPEN, RoomManager detects monsters → ARMED → LOCKED
5. Combat proceeds normally

**Why CLOSED not LOCKED:** LOCKED doors are opened by monster kill (auto-unlock). CLOSED doors are opened by E key (player action). Challenge Room needs player-initiated opening.

### Q4: Monster强化

**Decision: ChallengeModifier struct + GrowthCurve reuse.**

```cpp
struct ChallengeModifier {
    float hp_mult = 1.5f;    // base HP multiplier
    float atk_mult = 1.3f;   // base ATK multiplier
    int   extra_buffs = 1;   // additional random buffs per monster
};
```

Applied AFTER `spawn_monster()` + GrowthCurve scaling:
```cpp
m->combat.max_hp = (int)(m->combat.max_hp * gc.monstner_hp * modifier.hp_mult);
m->combat.attack = (int)(m->combat.attack * gc.monster_atk * modifier.atk_mult);
// + extra random buffs from elite_buff_pool
```

**Reuses existing systems:**
- `spawn_monster()` for base monster creation
- `GrowthCurve` for floor scaling
- `elite_buff_pool` for buff assignment

### Q5: Wave System

**Decision: Timer-based wave spawner (simplest).**

```cpp
struct ChallengeWave {
    int monster_count = 4;
    float spawn_delay = 3.0f;  // seconds between waves
};

struct ChallengeState {
    bool active = false;
    int current_wave = 0;
    int total_waves = 3;
    float wave_timer = 0.0f;
    bool waiting_for_next_wave = false;
};
```

**State machine:**
```
[Door opened] → spawn_wave(1) → LOCKED
    ↓
All wave 1 dead → waiting_for_next_wave = true, wave_timer = 3.0
    ↓
wave_timer <= 0 → spawn_wave(2)
    ↓
All wave 2 dead → wave_timer = 3.0
    ↓
wave_timer <= 0 → spawn_wave(3)
    ↓
All wave 3 dead → CLEARED
```

**Spawn safety:**
- Max 12 monsters alive at once (3 waves × 4)
- Spawn positions: random offset from room center, `is_walkable()` + no player overlap check (acceptable for MVP)
- Deterministic: wave composition is seeded by floor + room index

### Q6: Rewards

**Decision: Rarity-biased item drops + gold bonus.**

```cpp
// On CLEARED:
for (int i = 0; i < 3; i++) {
    auto item = generate_random_item();
    // Re-roll if COMMON (up to 3 times)
    int tries = 0;
    while (item && item->rarity == Rarity::COMMON && tries < 3) {
        item = generate_random_item(); tries++;
    }
    if (item) drop_item(item, room_center);
}
// Gold bonus: 50 + floor * 15
RewardManager::grant_gold(player, 50 + floor * 15);
```

**Result:** 3 items biased toward RARE+, plus gold. High risk → high reward.

### Q7: Failure

**Decision: Standard death. No special failure handling.**

Once the player opens the Challenge Room door:
- It's a real combat encounter
- Player death = normal game over
- No "retreat" option
- No key refund

This is intentional — Challenge Room is optional, high-risk, high-reward.

---

## Part 3: File Changes

### New Files

| File | Purpose | Est. Lines |
|------|---------|-----------|
| `src/game/world/challenge_room.h` | `ChallengeState`, `ChallengeModifier` structs | ~30 |
| `src/game/world/challenge_room.cpp` | Wave spawning, timer logic, reward granting | ~80 |
| `tests/economy/challenge_room_test.cpp` | Unit tests | ~60 |

### Modified Files

| File | Changes | Est. Lines |
|------|---------|-----------|
| `special_room.h` | Add `CHALLENGE` to enum | +1 |
| `special_room.cpp` | Add `_exec_challenge()` stub + string conversions | +5 |
| `dungeon_generator.cpp` | Distance-based placement near exit | +15 |
| `interaction_handler.cpp` | Challenge Room E-key: key check → spend_key → open | +10 |
| `room_manager.cpp` | Support wave-spawned monsters (minor) | +5 |
| `game_renderer.cpp` | Key hint in Challenge Room ("需要钥匙" or "E开门") | +3 |
| `game_scene.cpp` | ChallengeRoom state tick in main loop | +10 |
| `floor_config.cpp` | Challenge Room enabled on floors ≥ 3 | +2 |

**Total: ~220 lines across 11 files**

---

## Part 4: What We're NOT Doing

- ❌ Rewriting BSP for adjacency
- ❌ New door state types
- ❌ Complex wave composition (just count-based)
- ❌ Animated door unlock sequence
- ❌ Challenge Room on boss floors
- ❌ Multiple difficulty tiers
- ❌ Leaderboard / score tracking
- ❌ Special Challenge Room music

---

## Part 5: Review Gate Questions

### Q1: CHALLENGE enum value
Add `CHALLENGE` as 11th `SpecialRoomType` value. Minimal change, no existing logic disrupted. **Approve?**

### Q2: Key consumption
First real use of `spend_key()`. Keys currently accumulate from Gambler room (20% roll). After Batch 3D, keys will have a meaningful sink. **Approve?**

### Q3: No retreat
Once the door opens, it's a fight to the death. No key refund, no early exit. **Approve?**

### Q4: 3 waves × 4 monsters = 12 max
Is 12 simultaneous monsters acceptable for performance? Current floor cap is 4-8 normal monsters. **Approve or adjust?**

### Q5: Timer-based waves (3s delay between)
Simple timer between waves. No "all dead" detection — just a fixed delay. **Approve or prefer wave-triggered?**

### Q6: Rarity-biased rewards
3 items with COMMON re-roll (up to 3 tries each) + gold. Not guaranteed RARE+, but strongly biased. **Approve?**

---

*Architecture audit complete. Awaiting Review Gate before coding.*
