# Special Rooms Expansion — Architecture Audit

> **Phase:** Design / Audit Only — NO production code modifications
> **Date:** 2026-08-28
> **Current Version:** v1.2.6 (44/44 ctest, 295 source files)

---

## Executive Summary

This audit examines the feasibility of adding two new special room types — **Gamble Room** (gambling with sell/gold/roll mechanics) and **Challenge Room** (optional high-risk/high-reward key-locked encounter) — to the existing Roguelike dungeon system.

### Key Findings

| Finding | Severity | Impact |
|---------|----------|--------|
| **No Key item system exists** | CRITICAL | Challenge Room requires new item type + inventory branch + save/load |
| **No Gold/Currency system exists** | CRITICAL | Gamble Room requires new currency from scratch |
| **Relics are NOT persistent across floors** | HIGH | Gamble Room relic rewards need `from_boss=true` flag or new persistence |
| **No wave-based monster spawning** | HIGH | Challenge Room multi-wave combat requires new system |
| **No unified Reward API** | MEDIUM | Boss/Gamble/Challenge each would have separate drop logic |
| **BSP tree has no adjacency graph** | MEDIUM | Challenge Room placement near exit requires new placement algorithm |
| **LANDMARK enum has incomplete handling** | LOW | Existing bug, not blocking but should be fixed |

### Verdict

**Both rooms are architecturally feasible** but require non-trivial new infrastructure. The Challenge Room is more complex (needs Key system + wave spawning + difficulty scaling), while the Gamble Room is simpler (needs Gold system + sell mechanic + reuse existing reward).

---

## 1. Current Architecture Map

### 1.1 SpecialRoom System

| File | Class/Struct | Responsibility |
|------|-------------|----------------|
| `src/game/world/special_room.h` | `enum SpecialRoomType` | 10 values: ALTAR, TREASURE, FOUNTAIN, SHOP, BLACKSMITH, LIBRARY, GAMBLER, SHRINE, SECRET, LANDMARK |
| `src/game/world/special_room.h` | `struct SpecialRoom` | Runtime instance: type, rect, triggered, cx/cy, landmark_id, biome_id |
| `src/game/world/special_room.cpp` | `execute_special_room()` | Interaction dispatch — 10 switch cases, each a static function |
| `src/game/world/dungeon_generator.h` | `DungeonGenerator` | BSP partition → rooms → corridors → template → doors → special rooms |
| `src/game/world/dungeon_generator.cpp` | `_assign_special_rooms()` | Picks rooms 1..N-2, shuffles, cycles type_idx, ~50% landmark, 30% SECRET |
| `src/game/world/game_map.h` | `GameMap::special_rooms` | `std::vector<SpecialRoom>` (public) |
| `src/game/systems/interaction_handler.cpp` | `InteractionHandler` | E key → `get_special_room_at()` → `execute_special_room()` |

**Current room type count:** 10 (ALTAR through LANDMARK)
**New additions needed:** GAMBLE_ROOM, CHALLENGE_ROOM (2 new enum values)

### 1.2 DungeonGenerator Flow

```
generate(seed, special_room_count, arena_density, biome_id)
  │
  ├─ (1) Seed RNG
  ├─ (2) Clear state
  ├─ (3) new BSPNode(root)
  ├─ (4) _partition(root)        ← Recursive BSP split
  ├─ (5) _create_rooms(root)     ← Room rects in leaf nodes → flat _rooms vector
  ├─ (6) _connect_rooms(root)    ← CorridorConnection pairs (room_a_edge → door_a → door_b → room_b_edge)
  ├─ (7) _build_template()       ← Char grid: '#'=wall, '.'=floor, 'D'=door
  ├─ (8) _repair_room_apertures() ← Seal gaps, ensure DOOR-only apertures
  ├─ (9) gm->load_from_template()
  ├─ (10) _assign_special_rooms() ← Room 0 excluded, last room excluded
  ├─ (11) gm->special_rooms = _special_rooms
  └─ (12) _assign_arena_objects()
```

**Critical observations:**
- Room 0 = spawn (always excluded from special assignment)
- `_rooms.back()` = boss/exit room (always excluded)
- No explicit exit room — stairs tile placed at `_rooms.back()` center
- No adjacency graph — connectivity is implicit in BSP tree topology
- `_repair_room_apertures()` ensures DOOR is the only boundary opening (INVARIANT)

### 1.3 RoomManager State Machine

```
IDLE → ARMED → LOCKED → CLEARED
         ↑        │
         └────────┘  (player on door → stays ARMED)
```

- `IDLE`: No monsters / already cleared / boss room (skipped)
- `ARMED`: Player entered, monsters alive, waiting to lock
- `LOCKED`: Doors CLOSED, combat in progress
- `CLEARED`: All monsters dead, doors OPEN

**Boss rooms stay IDLE** — never lock, never auto-clear.

### 1.4 GameMap Door API

| Method | Effect |
|--------|--------|
| `close_room_doors(door_tiles)` | Atomic set all → CLOSED |
| `lock_room_doors(door_tiles)` | Atomic set all → LOCKED |
| `open_room_doors(door_tiles)` | Atomic set all → OPEN |

DoorState: NONE → OPEN → CLOSED → LOCKED → SEALED

---

## 2. Existing Reusable Systems

| System | Gamble Room | Challenge Room | Reusable? |
|--------|-------------|----------------|-----------|
| SpecialRoom enum/struct | ✅ Add GAMBLE_ROOM | ✅ Add CHALLENGE_ROOM | ✅ Direct extend |
| execute_special_room() | ✅ Add case | ⚠️ Partial — needs wave spawn | ⚠️ Partial |
| Key System | ❌ Not needed | ❌ Does NOT exist | ❌ Must create |
| Gold/Currency | ❌ Does NOT exist | ❌ Not needed | ❌ Must create |
| Equipment | ✅ Sell/generate | ✅ Reward | ✅ Direct reuse |
| Relic | ✅ Gamble reward | ✅ High-tier reward | ⚠️ Needs persistence flag |
| Reward Pipeline | ⚠️ No unified API | ⚠️ No unified API | ⚠️ Should create |
| RoomManager | ⚠️ Reuse for encounter | ✅ Reuse LOCKED/CLEARED | ✅ Direct reuse |
| Monster Spawn | ❌ No wave system | ❌ No wave system | ❌ Must create |
| DoorState | ✅ Already LOCKED | ✅ Already LOCKED | ✅ Direct reuse |
| DungeonGenerator | ⚠️ Need new placement | ⚠️ Need new placement | ⚠️ Need extension |
| GrowthCurve | ✅ gold_scale (unused) | ✅ elite_scale | ✅ Direct reuse |
| Rarity weights | ✅ random_rarity() | ✅ random_rarity() | ✅ Direct reuse |
| generate_random_item() | ✅ For gamble rolls | ✅ For rewards | ✅ Direct reuse |
| _try_grant_random_relic() | ✅ For gamble relic | ✅ For challenge relic | ✅ Direct reuse |

---

## 3. Detailed Audit Results

### 3.1 Key System — DOES NOT EXIST

**Critical finding:** There is no key item type in the entire codebase.

| Aspect | Status | Details |
|--------|--------|---------|
| Key item type | ❌ Missing | No enum, struct, or JSON definition |
| Key in inventory | ❌ Missing | Inventory only handles Equipment/Consumable/Charm |
| Key obtain | ❌ Missing | No code path adds key items |
| Key consume | ❌ Missing | No code path checks or spends keys |
| Multiple keys | ❌ Missing | N/A |
| Key save/load | ❌ Missing | Save format has no key branch |
| Door LOCKED + Key | ❌ Missing | LOCKED doors auto-unlock on monster kill, never use keys |

**Existing "key-adjacent" concept:** `DoorState::LOCKED` exists but is purely encounter-driven (ARMED→LOCKED→CLEARED). Player never opens LOCKED doors with an item.

**Implication for Challenge Room:** A Key item system must be created from scratch, including:
- New `ItemType::KEY` or `KeyItem` class
- Inventory support (stacking? single-use?)
- Obtain mechanism (monster drop? shop? event?)
- Consume mechanism (Challenge Room gate)
- Save/Load serialization branch
- UI display

### 3.2 Gold/Currency System — DOES NOT EXIST

**Critical finding:** There is no spendable in-run currency.

| Aspect | Status | Details |
|--------|--------|---------|
| Gold coins | ❌ Missing | `gold_scale` in GrowthCurve is a drop rate modifier, not spendable |
| Player gold field | ❌ Missing | No `player->gold` member |
| Shop pricing | ❌ Missing | SHOP room gives free items |
| Sell mechanic | ❌ Missing | No item→gold conversion |
| Gold HUD | ❌ Missing | No gold counter in UI |

**Meta-currencies** exist (`soul_fragments`, `knowledge`, `ancient_memory`) but are cross-run only, stored in `meta_save.json`.

**Implication for Gamble Room:** A Gold system must be created from scratch, including:
- New `Player::gold` field (or separate Gold component)
- Sell mechanic: `Inventory::sell(index)` → returns gold value
- Item value/pricing: `EquipmentItem::sell_value` based on rarity
- Gamble cost: gold → random reward
- Gold HUD display
- Save/Load serialization
- Gold drop from monsters (optional, or sell-only)

### 3.3 Relic Persistence

**Current behavior (B13):**
```
Floor N: Player has relics [A, B, C] (from_boss=false) + [D] (from_boss=true)
    ↓
Floor N+1: Only [D] survives. A, B, C stripped.
```

**Boss relics survive** (`from_boss=true`). All others are cleared on floor enter.

**Implication for Gamble Room:**
- If Gamble Room gives a relic, it should set `from_boss=true` to persist
- OR: create a new flag `from_gamble=true` with same persistence behavior
- OR: unify into a `persistent` flag on `RelicInstance`

**No unified reward API exists.** Relic granting is copy-pasted across 7+ sources:
- `_try_grant_random_relic()` (special_room.cpp) — rarity-weighted + build affinity
- Boss kill (game_scene_combat.cpp) — uniform random, from_boss=true
- Event system (event_system.cpp) — ad-hoc per event type
- Quest rewards (quest_manager.cpp) — direct id grant
- JSON encounters — string-parsed "relic:1"

### 3.4 Equipment Rarity & Drop System

**Rarity tiers:** COMMON(0), RARE(1), EPIC(2), LEGENDARY(3)

**JSON weights:** `{60, 25, 12, 3}` → 60% common, 25% rare, 12% epic, 3% legendary

**JSON multipliers:** `{1.0, 1.2, 1.5, 2.0}` (applied to armor/charm, NOT weapons)

**`generate_random_item()`** — unified factory, picks category (weapon:35.7%, armor:35.7%, potion:14.3%, charm:14.3%), then rarity, then random template.

**`random_rarity()`** — weighted roll from RarityConfig.

**GrowthCurve `gold_scale`** — drop chance multiplier per floor (1.0→2.6). Currently **unused for actual gold** (misnomer).

**No RewardContext / quality_bonus system.** No way to temporarily shift rarity weights for a specific source.

**Implication for Challenge Room:**
- Can reuse `generate_random_item()` directly
- To shift rarity for high-quality rewards: either
  a. Create `generate_random_item(min_rarity)` parameter, or
  b. Roll rarity separately with boosted weights, then pass to factory
  c. Simplest: `generate_random_item()` + post-roll re-roll if below threshold

### 3.5 Monster Wave System — DOES NOT EXIST

**Current spawning:**
- Bulk spawn at floor enter (`FloorManager::spawn_floor_monsters`)
- Dynamic patrol spawn (`FlowDirector::auto_spawn_suggestion`)
- Elite auto-spawn (20% chance on combat timer)
- Boss arena summon (BossAI skill)

**No wave-based system.** No concept of "Wave 1 → Wave 2 → Wave 3".

**Scaling exists:**
- `GrowthCurve`: `monster_hp`, `monster_atk`, `elite_scale` per floor
- Elite: +50% XP, +50% loot, random buff from `elite_buff_pool`
- No Champion tier, no MonsterModifier class

**Implication for Challenge Room:**
- Need a simple wave spawner: timer-based, spawn N monsters per wave, M waves
- Can reuse `spawn_monster()` factory
- Apply `elite_scale` or custom multiplier for enhanced difficulty
- Minimal extension: `ChallengeWaveDef { int count; float delay; std::vector<std::string> enemies; }`

### 3.6 Challenge Room Placement

**Current layout constraint:**
```
_rooms[0] = spawn
_rooms[1..N-2] = special rooms / normal rooms
_rooms.back() = boss/exit (stairs at center)
```

**No adjacency graph.** BSP tree topology determines connectivity implicitly.

**INVARIANT:** `_repair_room_apertures()` ensures DOOR is the only boundary opening.

**Challenge Room requirements:**
1. Must be optional (not blocking exit reachability)
2. Must be near exit room
3. Key gate at entrance
4. Does not break seal invariant

**Placement options:**

**Option A: Dedicated BSP partition** — Add a small extra room adjacent to `_rooms.back()` during generation. This requires modifying `_connect_rooms()` to add an extra corridor to the challenge room.

**Option B: Steal a candidate room** — During `_assign_special_rooms()`, reserve the room closest to `_rooms.back()` as the challenge room candidate. Risk: may not always be adjacent.

**Option C: Post-generation attachment** — After BSP generation, manually carve a small room next to `_rooms.back()` and connect it via a corridor. Similar to how ArenaObject works but for rooms.

**Option A is cleanest** but modifies DungeonGenerator. **Option C is safest** (no BSP changes) but requires template manipulation.

### 3.7 Room Encounter Reuse

**Three options for Challenge Room encounter:**

| Option | Approach | Pros | Cons |
|--------|----------|------|------|
| **A: Reuse RoomManager** | Challenge Room = LOCKED room with special spawn | Minimal new code, existing state machine | No wave control, single lock/unlock cycle |
| **B: New ChallengeEncounter** | Separate class for wave management | Full wave control, custom rewards | More code, potential duplication |
| **C: Extend RoomEncounterType** | Add CHALLENGE to existing system | Unified, extensible | Modifies existing enum, risk of breaking |

**Recommendation: Option A + lightweight wave hook.** Use RoomManager for lock/unlock, but spawn monsters in waves via a timer callback. This avoids duplicating the lock/clear logic while enabling multi-wave combat.

---

## 4. Architecture Gaps

### Already Exists (Direct Reuse)
- SpecialRoom enum/struct (add 2 values)
- execute_special_room() dispatch (add 2 cases)
- Equipment/Item generation (`generate_random_item()`)
- Relic generation (`_try_grant_random_relic()`)
- Rarity weighting (`random_rarity()`)
- RoomManager lock/clear state machine
- DoorState LOCKED/CLOSED/OPEN
- GrowthCurve scaling
- Monster spawn factory (`spawn_monster()`)
- Elite system (`is_elite`, `elite_buff_pool`)
- BuildScore affinity for relics

### Needs Small Extension
- `SpecialRoomType` enum: +2 values (GAMBLE_ROOM, CHALLENGE_ROOM)
- `execute_special_room()`: +2 cases
- `special_room_from_index()`: update modulo
- `special_room_to_string()` / `from_string()`: +2 mappings
- `interaction_handler.cpp`: +2 discovery strings
- `game_map.cpp` draw(): +2 floor colors + icons
- `generate_random_item()`: optional `min_rarity` parameter (or re-roll approach)
- `RelicInstance`: add `persistent` flag (or reuse `from_boss`)
- DungeonGenerator: new placement logic for challenge room

### Missing (New Systems Required)
- **Key item system**: type, inventory, obtain, consume, save/load, UI
- **Gold/Currency system**: player field, sell, price, gamble cost, HUD, save/load
- **Wave spawner**: timer-based multi-wave monster spawn for challenge room
- **Reward pipeline** (optional but recommended): unified `grant_reward(source, context)` to avoid duplication

---

## 5. Decision Questions

### D1: Can Gamble Room be implemented on existing systems?
**Yes, but requires Gold system from scratch.** The sell→gold→gamble→reward flow needs:
- `Player::gold` field
- Item sell value (based on rarity)
- Gamble cost (fixed or scaling)
- Gamble roll (reuse `generate_random_item()` + `_try_grant_random_relic()`)

### D2: Can Challenge Room be placed near exit?
**Yes, with a new placement algorithm.** Options:
- Post-generation room carving next to `_rooms.back()` (safest)
- Dedicated BSP partition (cleanest but modifies generator)

### D3: Can Key system be reused?
**No — it doesn't exist.** Must create from scratch. Minimal design:
- `KeyItem : Item` (single type, stackable)
- `Inventory::use_key()` or direct consumption
- Monster drop / shop / event as source
- Save/load branch in `save_manager.cpp`

### D4: Can Relic rewards be unified?
**No unified API exists.** Current state is 7+ copy-pasted paths. Recommendation: Create `RewardPipeline::grant_relic(source, player)` but this is optional for MVP — can keep ad-hoc for now.

### D5: Should Challenge Encounter reuse RoomManager?
**Yes.** RoomManager's IDLE→ARMED→LOCKED→CLEARED is sufficient. The wave spawning is orthogonal — spawn monsters on a timer while RoomManager handles lock/unlock. No need for ChallengeEncounter class.

### D6: What's the minimal implementation path?
See Section 6 (Proposed Batches).

---

## 6. Proposed Implementation Batches

### Batch SR1 — Key Item System
**Scope:** New `KeyItem` class, inventory support, save/load, basic obtain (monster drop chance)
**Files:** `item.h`, `item.cpp`, `inventory.h`, `inventory.cpp`, `save_manager.cpp`, `enemies.json`
**Size:** ~120 lines new code
**Risk:** Medium — touches inventory and save system
**Depends on:** Nothing

### Batch SR2 — Gold/Currency System
**Scope:** `Player::gold` field, sell mechanic, item sell_value, gold HUD, save/load
**Files:** `player.h`, `inventory.cpp`, `game_renderer.cpp`, `save_manager.cpp`
**Size:** ~100 lines new code
**Risk:** Low — additive, no existing logic changes
**Depends on:** Nothing

### Batch SR3 — Challenge Room Generation
**Scope:** Post-generation room carving next to exit, CHALLENGE_ROOM enum, key gate tile, DungeonGenerator extension
**Files:** `special_room.h`, `dungeon_generator.cpp`, `game_map.cpp`
**Size:** ~80 lines new code
**Risk:** Medium — touches generation pipeline
**Depends on:** Batch SR1 (Key system for gate)

### Batch SR4 — Challenge Room Encounter
**Scope:** Wave spawner (3 waves, timer-based), enhanced monster scaling, RoomManager integration
**Files:** `special_room.cpp`, `room_manager.cpp`, `game_scene.cpp`
**Size:** ~120 lines new code
**Risk:** Medium — new spawning logic
**Depends on:** Batch SR3

### Batch SR5 — Challenge Room Reward
**Scope:** High-quality reward on clear (rarity boost, guaranteed relic)
**Files:** `special_room.cpp`
**Size:** ~40 lines new code
**Risk:** Low — reuses existing `generate_random_item()` + `_try_grant_random_relic()`
**Depends on:** Batch SR4

### Batch SR6 — Gamble Room Economy
**Scope:** Sell item → gain gold, gamble cost, random reward roll
**Files:** `special_room.cpp`, `item.h`
**Size:** ~80 lines new code
**Risk:** Low — self-contained in execute_special_room()
**Depends on:** Batch SR2 (Gold system)

### Batch SR7 — Gamble Room UI
**Scope:** Gamble panel (sell list, gold display, gamble button, result display)
**Files:** `game_renderer.cpp`, `special_room.cpp`
**Size:** ~100 lines new code
**Risk:** Low — UI overlay
**Depends on:** Batch SR6

### Batch SR8 — Integration & Tests
**Scope:** Full integration test, key drop test, gold economy test, challenge room e2e
**Files:** `tests/`, various
**Size:** ~150 lines test code
**Risk:** Low
**Depends on:** All above

---

## 7. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Key system complexity | High | High | Keep it minimal — single key type, no stacking, simple consume |
| Gold economy balance | Medium | Medium | Start with sell-only (no monster gold drops), tune later |
| Wave spawner performance | Low | Low | Max 3 waves × 5 monsters = 15 entities, well within budget |
| BSP modification breaks determinism | Medium | High | Use post-generation carving (Option C) to avoid BSP changes |
| Save/load format change | Medium | Medium | Add version flag, backward-compatible parsing |
| Challenge Room too easy/hard | High | Medium | GrowthCurve already provides scaling; tune per-wave multiplier |
| Gamble Room exploits | Low | Low | Fixed costs, no save-scum (deterministic seed) |

---

## 8. Final Recommendation

### Phase 1 (MVP): Gamble Room Only
- Batch SR2 (Gold) → Batch SR6 (Economy) → Batch SR7 (UI)
- **Rationale:** Simpler, self-contained, no Key system needed, no generation changes
- **Estimated effort:** 3 batches, ~260 lines

### Phase 2: Challenge Room
- Batch SR1 (Key) → Batch SR3 (Generation) → Batch SR4 (Encounter) → Batch SR5 (Reward)
- **Rationale:** Depends on Key system which is non-trivial
- **Estimated effort:** 4 batches, ~360 lines

### Phase 3: Polish & Tests
- Batch SR8 (Integration)
- **Estimated effort:** 1 batch, ~150 lines

### Total estimated new code: ~770 lines (across 8 batches)

---

## Appendix A: File Reference

| File | Lines | Role |
|------|-------|------|
| `src/game/world/special_room.h` | 31 | SpecialRoomType enum + struct |
| `src/game/world/special_room.cpp` | 371 | All room interaction implementations |
| `src/game/world/dungeon_generator.h` | 96 | BSPNode, DungeonGenerator class |
| `src/game/world/dungeon_generator.cpp` | 443 | Generation pipeline |
| `src/game/world/game_map.h` | 123 | GameMap with special_rooms vector |
| `src/game/world/game_map.cpp` | 457 | Map rendering, door API, room lookup |
| `src/game/world/room_manager.h` | 73 | RoomManager state machine |
| `src/game/world/room_manager.cpp` | 142 | Lock/clear logic |
| `src/game/entities/inventory.h` | 31 | Inventory class |
| `src/game/entities/inventory.cpp` | 64 | Add/remove/equip/use |
| `src/game/entities/item.h` | 97 | Item hierarchy + Rarity enum |
| `src/game/entities/item.cpp` | 248 | Rarity helpers, generate_random_item() |
| `src/game/entities/monster.h` | 105 | Monster class |
| `src/game/entities/monster.cpp` | 387 | spawn_monster() factory |
| `src/game/world/growth_curve.h` | 40 | GrowthCurve struct |
| `src/game/world/growth_curve.cpp` | 60 | 15-floor scaling table |
| `src/game/scene/game_scene_combat.cpp` | 160 | on_monster_killed() drops |
| `src/game/systems/interaction_handler.cpp` | 79 | E key special room interaction |
| `src/game/save/save_manager.cpp` | 386 | Save/load (no key/relic support) |

## Appendix B: Existing SpecialRoom Implementations

| Room Type | Interaction | Items Given | Relic? |
|-----------|------------|-------------|--------|
| ALTAR | Buff/debuff choice | None | No |
| TREASURE | Chest open | 1-2 items | 40-100% chance |
| FOUNTAIN | Heal + cleanse | None | No |
| SHOP | Free items | 2 items (rare+) | No |
| BLACKSMITH | Upgrade weapon | None | No |
| LIBRARY | Learn skill | None | No |
| GAMBLER | Gambling minigame | 1 item | 50% chance |
| SHRINE | Blessing | Buff | 50% chance (1/3 trigger) |
| SECRET | Hidden room | 1 item + buff | 100% chance |
| LANDMARK | Biome-specific | Varies | Varies |

---

*Audit complete. Awaiting Review Gate before implementation.*
