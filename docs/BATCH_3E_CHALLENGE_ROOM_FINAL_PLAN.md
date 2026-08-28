# Batch 3E — Challenge Room Final Implementation Plan

> **Status:** Design Phase — Awaiting Review Gate
> **Date:** 2026-08-28
> **Predecessor:** Batch 3D Architecture Audit
> **Scope:** Frozen implementation plan for Challenge Room

---

## 1. State Machine

```
INACTIVE        — room generated, door CLOSED, no interaction
    ↓ [E + Key]
UNLOCKED        — Key consumed, door OPEN, player outside room
    ↓ [player enters room]
ARMED           — RoomManager detects monsters, doors LOCKED
    ↓ [wave spawn triggered]
WAVE_SPAWNING   — monsters spawning (instant, within 1 frame)
    ↓ [spawn complete]
COMBAT          — waiting for all current wave monsters to die
    ↓ [all wave monsters dead]
WAIT_NEXT_WAVE  — 3 second timer counting down
    ↓ [timer <= 0, more waves remaining]
WAVE_SPAWNING   — next wave spawns
    ↓ [all 3 waves cleared]
REWARD          — reward drops + gold grant
    ↓ [reward delivered]
CLEARED         — doors OPEN, room permanently done
```

**Transitions driven by:** `ChallengeRoomController::tick()`
**Door state managed by:** `RoomManager` (LOCKED/CLEARED) and `GameMap` (OPEN/CLOSED)

---

## 2. Class: ChallengeRoomController

**File:** `src/game/world/challenge_room.h` + `challenge_room.cpp`

```cpp
enum class ChallengePhase : uint8_t {
    INACTIVE, UNLOCKED, ARMED,
    WAVE_SPAWNING, COMBAT, WAIT_NEXT_WAVE,
    REWARD, CLEARED
};

struct ChallengeModifier {
    float hp_multiplier = 1.5f;
    float attack_multiplier = 1.3f;
    int extra_buffs = 1;
};

struct ChallengeWave {
    int monster_count = 4;
    float spawn_delay = 3.0f;
};

class ChallengeRoomController {
public:
    void reset();
    bool try_activate(Player& player);   // check key, spend, open door
    void tick(float dt, GameMap* map, Player* player,
              std::vector<std::unique_ptr<Monster>>& monsters);

    ChallengePhase phase() const;
    bool is_cleared() const;

private:
    ChallengePhase _phase = ChallengePhase::INACTIVE;
    int _current_wave = 0;
    int _total_waves = 3;
    float _wave_timer = 0.0f;
    int _monsters_alive_this_wave = 0;

    void _spawn_wave(int wave_index, GameMap* map,
                     std::vector<std::unique_ptr<Monster>>& monsters);
    void _grant_rewards(Player& player, GameMap* map);
};
```

**Responsibilities:**
- Phase transitions (state machine)
- Wave spawning via `spawn_monster()` + `ChallengeModifier`
- Monster alive counting
- Timer management
- Reward granting via `RewardManager`

**NOT responsible for:**
- Door state (RoomManager handles LOCKED/CLEARED)
- Monster AI
- Player movement
- Save/load

---

## 3. Spawn Rules

### Valid spawn positions

Monsters must satisfy ALL:
- `_in_bounds(tx, ty)` — within map
- `is_walkable(tx, ty)` — not a wall
- Tile type is FLOOR — not DOOR, not STAIRS
- Distance from player > 2 tiles — no spawn on top of player

### Spawn algorithm

```
for each monster in wave:
    attempts = 0
    while attempts < 50:
        pick random room center from floor rooms
        offset = random [-2, +2] tiles
        tx = center.x + offset.x
        ty = center.y + offset.y
        if valid_position(tx, ty, player_pos):
            spawn_monster(tx, ty, random_type)
            apply ChallengeModifier
            attempts++
            break
        attempts++
    if attempts >= 50:
        skip this monster (safety fallback)
```

### Monster types

Random from floor-appropriate pool (reuse `FloorConfig::enemy_weights`):
- F1-5: slime, orc, bomber
- F6-10: + charger, tank, summoner
- F11-15: + all types including elites

### Hard limit

```cpp
static constexpr int MAX_CHALLENGE_MONSTERS = 12;
```

Never spawn if `_monsters_alive_this_wave >= MAX_CHALLENGE_MONSTERS`.

---

## 4. ChallengeModifier Application

```cpp
// After spawn_monster():
const GrowthCurve& gc = g_growth.curve(floor);
m->combat.max_hp = (int)(m->combat.max_hp * gc.monster_hp * modifier.hp_multiplier);
m->combat.current_hp = m->combat.max_hp;
m->combat.attack = (int)(m->combat.attack * gc.monster_atk * modifier.attack_multiplier);
// Extra buffs
for (int i = 0; i < modifier.extra_buffs; i++) {
    // apply random buff from elite_buff_pool
}
```

**Reuses:** `spawn_monster()`, `GrowthCurve`, `elite_buff_pool`

---

## 5. Reward System

### On REWARD phase:

```cpp
void ChallengeRoomController::_grant_rewards(Player& player, GameMap* map) {
    // 1. Guaranteed: 3× RARE+ items
    for (int i = 0; i < 3; i++) {
        auto item = generate_random_item();
        int tries = 0;
        while (item && item->rarity < Rarity::RARE && tries < 5) {
            item = generate_random_item(); tries++;
        }
        if (item) {
            if (player.inventory.add(item, &player)) {
                // Item added to inventory
            } else {
                // Inventory full → ground drop near room center
                _drop_ground_reward(item, map);
            }
        }
    }

    // 2. Gold: floor-scaled
    int gold = 50 + player.current_floor * 15;
    RewardManager::grant_gold(player, gold);
}
```

**Inventory-full handling:** If `inventory.add()` fails, items drop on the ground as `DroppedItem` in the challenge room center. Rewards never silently disappear.

---

## 6. Door Interaction Flow

```
[E] pressed in INACTIVE room:
    1. Check player->key_count > 0
    2. If no key → message "需要钥匙开门" → return
    3. player->spend_key(1)
    4. GameMap::open_room_doors(door_tiles) → doors become OPEN
    5. Set phase = UNLOCKED
    6. Message: "挑战开始！"

[Player enters room bounds]:
    7. RoomManager detects ARMED (monsters present)
    8. RoomManager::lock_room_doors() → doors become LOCKED
    9. Set phase = ARMED → WAVE_SPAWNING → spawn Wave 1

[All wave monsters dead]:
   10. If more waves → WAIT_NEXT_WAVE (3s timer)
   11. If final wave done → REWARD → CLEARED
   12. RoomManager::open_room_doors() → doors OPEN
```

---

## 7. Precise File List

### New Files

| File | Responsibility | Est. Lines |
|------|---------------|-----------|
| `src/game/world/challenge_room.h` | `ChallengePhase`, `ChallengeModifier`, `ChallengeWave`, `ChallengeRoomController` class | ~35 |
| `src/game/world/challenge_room.cpp` | State machine, wave spawning, modifier application, reward granting | ~90 |
| `tests/world/challenge_room_test.cpp` | Unit tests | ~70 |

### Modified Files

| File | Change | Responsibility | Est. Lines |
|------|--------|---------------|-----------|
| `special_room.h` | Add `CHALLENGE` to enum | Type definition | +1 |
| `special_room.cpp` | Add `_exec_challenge()` (stub), string conversions | Room type support | +5 |
| `dungeon_generator.cpp` | Distance-based placement near exit | Placement algorithm | +18 |
| `interaction_handler.cpp` | `[E]` in CHALLENGE room → `ChallengeRoomController::try_activate()` | Key check + door open | +8 |
| `room_manager.cpp` | Minor: skip boss_room check for challenge rooms | Lock/Clear support | +3 |
| `game_scene.cpp` | Tick `ChallengeRoomController` in main loop | Game integration | +12 |
| `game_renderer.cpp` | Show "需要钥匙" or "E开门" in Challenge Room | UI feedback | +3 |

**Total new code: ~245 lines across 10 files**

---

## 8. Integration Points

### game_scene.cpp — Tick Loop

```cpp
// In GameScene::_update(), after RoomManager::tick():
if (_challenge_room_active) {
    _challenge_ctrl.tick(dt, game_map.get(), player.get(), monsters);
    if (_challenge_ctrl.is_cleared()) {
        _challenge_room_active = false;
    }
}
```

### interaction_handler.cpp — E-Key

```cpp
SpecialRoom* room = map->get_special_room_at(tx, ty);
if (room && room->type == SpecialRoomType::CHALLENGE) {
    if (challenge_ctrl.phase() == ChallengePhase::INACTIVE) {
        return challenge_ctrl.try_activate(*player);
    }
    return "";
}
```

### game_scene.cpp — Room State Detection

```cpp
// After RoomManager::tick():
if (challenge_ctrl.phase() == ChallengePhase::UNLOCKED) {
    // Check if player entered the room
    auto [ptx, pty] = map->pixel_to_tile(player position);
    if (challenge_ctrl.room_contains(ptx, pty)) {
        challenge_ctrl.on_player_entered();
    }
}
```

---

## 9. Test Plan

| Test | What It Verifies |
|------|-----------------|
| `ChallengeRequiresKey` | E with 0 keys → "需要钥匙开门", key_count unchanged |
| `ChallengeKeyConsumption` | E with 3 keys → key_count = 2, phase = UNLOCKED |
| `ChallengeWaveSequence` | 3 waves spawn in order, monsters_alive counts correct |
| `ChallengeMonsterCap` | Never exceeds MAX_CHALLENGE_MONSTERS (12) |
| `ChallengeModifierApplied` | Spawned monster HP = base × growth_curve × 1.5 |
| `ChallengeRewardGrant` | On CLEARED: items added to inventory, gold increased |
| `ChallengeOptional` | Floor can be cleared without entering Challenge Room |
| `ChallengeDeterminism` | Same seed → same wave composition + reward |
| `ChallengeDoorLock` | After player enters, doors become LOCKED |
| `ChallengeClearUnlocks` | After all waves, doors become OPEN |

---

## 10. Save/Load

**Challenge Room state is NOT saved across sessions.**

Rationale:
- Challenge Room is a single-floor encounter
- If player saves mid-challenge, monsters are already in the `monsters` vector (saved)
- Phase can be reconstructed: if room has alive monsters + LOCKED doors → re-enter COMBAT phase
- Simplification: on load, if Challenge Room is mid-challenge, reset to INACTIVE (monsters are lost)

**MVP decision: no cross-save challenge persistence.** Player must complete or abandon on the same session.

---

## 11. Determinism

Wave composition is seeded by deterministic hash (avoids XOR collision):

```cpp
// challenge_room.cpp
static uint32_t _challenge_seed(uint32_t dungeon_seed, int room_index, int wave_index) {
    uint32_t h = dungeon_seed;
    h ^= (uint32_t)(room_index + 1) * 0x9E3779B9u;
    h ^= (uint32_t)(wave_index + 1) * 0x85EBCA6Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16; h *= 0x45D9F3Bu;
    h ^= h >> 16;
    return h;
}
```

This ensures:
- Same dungeon_seed + same room + same wave → same composition
- No collision between (room=1,wave=2) and (room=2,wave=1)
- SimAI can reproduce challenge encounters

---

## 12. What's NOT in This Batch

- ❌ Difficulty tiers (easy/normal/hard)
- ❌ Timer pressure (wave time limit)
- ❌ Escape / retreat mechanic
- ❌ Challenge-specific music
- ❌ Visual wave counter UI
- ❌ Post-challenge score screen
- ❌ Challenge Room on boss floors
- ❌ Cross-save challenge persistence

---

## 13. Review Gate Questions

1. **ChallengeRoomController ownership** — Stored as member of GameScene. Acceptable?
2. **3s wave delay** — Fixed timer, not wave-clear-triggered. Acceptable?
3. **No cross-save** — Challenge resets on save/load. Acceptable?
4. **Determinism seed** — `hash_combine(dungeon_seed, room_index, wave_index)` with avalanche mix. Acceptable?
5. **Reward via RewardManager** — All grants go through RewardManager, ground fallback if inventory full. Acceptable?

---

*Final plan complete. Awaiting Review Gate before coding.*
