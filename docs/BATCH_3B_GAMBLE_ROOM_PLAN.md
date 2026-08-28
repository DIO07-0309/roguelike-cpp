# Batch 3B — Gamble Room MVP Implementation Plan

> **Status:** Design Phase — Awaiting Review Gate
> **Date:** 2026-08-28
> **Predecessor:** Batch 3A (Economy & Persistence Foundation)
> **Scope:** Gold-based Gamble Room MVP

---

## 1. Key Finding: GAMBLER Already Exists

The existing `SpecialRoomType::GAMBLER` (index 6) is a simple 60/40 HP-damage gambling:

```cpp
// Current: 60% win → random item, 40% lose → 20% HP damage
static std::string _exec_gambler(Player* player) {
    bool win = (rng() % 100 < 60);
    if (player_has_relic(player, "golden_dice")) win = (rng() % 100 < 85);
    if (win) {
        auto item = generate_random_item();
        if (item) { player->inventory.add(item, player); return "赌徒咧嘴一笑：运气不错！"; }
        return "赌徒摊手：今天没货了。";
    } else {
        int loss = std::max(1, player->combat.current_hp / 5);
        player->combat.take_damage(loss);
        return "赌徒摇头：命运不站在你这边。受到 " + std::string(...) + " 伤害。";
    }
}
```

**Decision: REPLACE this with gold-based gamble.** No new enum value needed. The existing GAMBLER type gets new behavior.

---

## 2. Architecture — What Changes

### 2.1 Problem: `triggered` Flag Blocks Repeated Use

Current flow in `interaction_handler.cpp`:
```cpp
SpecialRoom* room = map->get_special_room_at(tx, ty);
if (room && !room->triggered) {          // ← ONE-TIME LOCK
    std::string msg = execute_special_room(room->type, player);
    room->triggered = true;               // ← PERMANENT
    return msg;
}
```

**Gamble Room needs repeated use.** Fix: GAMBLER rooms bypass the `triggered` guard.

### 2.2 Solution: Modify `try_interact()` for GAMBLER

```cpp
SpecialRoom* room = map->get_special_room_at(tx, ty);
if (room) {
    // Gamble Room allows repeated interaction (gold-based)
    if (room->type == SpecialRoomType::GAMBLER || !room->triggered) {
        std::string msg = execute_special_room(room->type, player);
        if (room->type != SpecialRoomType::GAMBLER)
            room->triggered = true;
        return msg;
    }
}
```

**Impact:** Only 1 line change in `interaction_handler.cpp`. All other room types keep one-shot behavior.

---

## 3. New Gamble Room Behavior

### 3.1 Core Loop

```
Player enters GAMBLER room
    ↓
Press E → show cost message
    ↓
Gold >= cost?
    ├── YES → spend gold → roll reward → RewardManager
    └── NO  → "金币不足。需要 XX 金币。"
    ↓
Room stays active (repeatable)
```

### 3.2 Gamble Cost

```cpp
static int get_gamble_cost(int floor) {
    return 40 + floor * 10;
}
```

| Floor | Cost |
|-------|------|
| F1 | 50 |
| F5 | 90 |
| F10 | 140 |
| F15 | 190 |

### 3.3 Reward Pool (3-tier)

```cpp
int roll = rng() % 100;
if (roll < 70) {
    // 70% Equipment
    auto item = generate_random_item();
    RewardManager::grant_item(*player, item);
    return "获得了 " + item->get_description();
} else if (roll < 90) {
    // 20% Key
    RewardManager::grant_key(*player, 1);
    return "获得了一把钥匙！";
} else {
    // 10% RUN Relic
    // Use _try_grant_random_relic logic with PersistenceScope::RUN
    // ... (reuse existing rarity-weighted selection)
    return "RELIC:" + relic_name;
}
```

### 3.4 Relic Reward — RUN Persistence

Gamble Room relics must be `PersistenceScope::RUN` (same as Boss relics). Use `RewardManager::grant_relic()` with `PersistenceScope::RUN`.

---

## 4. File Changes

### Modified Files (4)

| File | Changes | Lines |
|------|---------|-------|
| `src/game/world/special_room.cpp` | Rewrite `_exec_gambler()`, add `get_gamble_cost()` | ~30 lines replaced |
| `src/game/systems/interaction_handler.cpp` | GAMBLER bypass `triggered` guard | ~3 lines |
| `src/game/world/special_room.cpp` | Update `_exec_gambler` discovery message (already exists) | 0 (keep) |
| `tests/economy/gamble_room_test.cpp` | New: gamble cost, gold spend, reward pool | ~50 lines |

### Unchanged Files (reuse as-is)

| File | What's Reused |
|------|---------------|
| `special_room.h` | `SpecialRoomType::GAMBLER` (already exists) |
| `game_map.cpp` | Floor color `{45, 15, 50}`, icon `"G"` (already exists) |
| `item.cpp` | `generate_random_item()`, `random_rarity()` |
| `reward_manager.cpp` | `grant_item()`, `grant_key()`, `grant_relic()` |
| `combat_system.cpp` | `player_has_relic()`, `get_relic_def()`, `get_all_relic_ids()` |
| `combat_stats.h` | `PersistenceScope::RUN` |

---

## 5. `_exec_gambler()` Rewrite

```cpp
static int _get_gamble_cost(int floor) {
    return 40 + floor * 10;
}

static std::string _exec_gambler(Player* player) {
    if (!player) return "";

    // Cost scales with current floor
    // NOTE: floor must be passed in or accessible
    // Solution: use player's current floor from GameScene, or pass as param
    // Simplest: store floor on Player (already available via GameScene)

    int cost = _get_gamble_cost(current_floor);  // needs floor access

    if (player->gold < cost) {
        return "金币不足。需要 " + std::to_string(cost) + " 金币。";
    }

    player->spend_gold(cost);

    int roll = rng() % 100;
    if (roll < 70) {
        // 70% Equipment
        auto item = generate_random_item();
        if (item && RewardManager::grant_item(*player, item)) {
            return "获得了 " + item->get_description();
        }
        return "抽奖池已空。";
    } else if (roll < 90) {
        // 20% Key
        RewardManager::grant_key(*player, 1);
        return "获得了一把钥匙！";
    } else {
        // 10% RUN Relic
        auto all_ids = get_all_relic_ids();
        std::vector<std::string> candidates;
        for (auto& id : all_ids)
            if (!player_has_relic(player, id))
                candidates.push_back(id);
        if (!candidates.empty()) {
            std::string chosen = candidates[rng() % candidates.size()];
            if (RewardManager::grant_relic(*player, chosen, PersistenceScope::RUN)) {
                const RelicDef* def = get_relic_def(chosen);
                return def ? ("RELIC:" + def->name) : "获得了一件圣物！";
            }
        }
        // Fallback: give key instead
        RewardManager::grant_key(*player, 1);
        return "圣物库已满，获得了一把钥匙作为替代。";
    }
}
```

---

## 6. Floor Access Problem

**Issue:** `_exec_gambler()` needs `current_floor` to calculate cost, but `execute_special_room()` only receives `Player*`.

**Solutions:**

| Option | Approach | Impact |
|--------|----------|--------|
| A | Add `current_floor` parameter to `execute_special_room()` | Changes signature, affects all callers |
| B | Store `current_floor` on Player | Player already has level/xp, add floor |
| C | Use `dungeon_seed` to derive floor (fragile) | Bad |
| **D** | Read floor from GameScene via global/static | Works but couples systems |

**Recommended: Option B** — Add `int current_floor = 1` to Player. Set on floor enter. Minimal change, no signature change.

```cpp
// player.h
int current_floor = 1;  // Batch 3B: current floor for Gamble Room cost

// game_scene.cpp (enter_floor)
player->current_floor = floor;
```

---

## 7. InteractionHandler Change

### File: `src/game/systems/interaction_handler.cpp`

**Before (line 20):**
```cpp
if (room && !room->triggered) {
```

**After:**
```cpp
if (room && (room->type == SpecialRoomType::GAMBLER || !room->triggered)) {
```

And line 22:
```cpp
// Before:
room->triggered = true;

// After:
if (room->type != SpecialRoomType::GAMBLER)
    room->triggered = true;
```

**Total: 2 lines changed.**

---

## 8. Test Plan

### New file: `tests/economy/gamble_room_test.cpp`

```cpp
TEST(GambleCostTest, CostScalesWithFloor) {
    // F1: 50, F5: 90, F10: 140, F15: 190
    EXPECT_EQ(_get_gamble_cost(1), 50);
    EXPECT_EQ(_get_gamble_cost(5), 90);
    EXPECT_EQ(_get_gamble_cost(10), 140);
    EXPECT_EQ(_get_gamble_cost(15), 190);
}

TEST(GambleRoomTest, InsufficientGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.gold = 30;
    p.current_floor = 1;
    // _exec_gambler should return "金币不足" message
}

TEST(GambleRoomTest, SpendGoldOnSuccess) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.gold = 100;
    p.current_floor = 1;
    // After gamble: gold should be 50 (100 - 50 cost)
}
```

---

## 9. File Change Summary

| File | Change | Est. Lines |
|------|--------|-----------|
| `src/game/world/special_room.cpp` | Rewrite `_exec_gambler()` + add `_get_gamble_cost()` | ~30 |
| `src/game/systems/interaction_handler.cpp` | GAMBLER bypass triggered guard | ~3 |
| `src/game/entities/player.h` | Add `int current_floor = 1` | +1 |
| `src/game/scenes/game_scene.cpp` | Set `player->current_floor` on floor enter | +1 |
| `tests/economy/gamble_room_test.cpp` | New test file | ~40 |

**Total: ~75 lines changed/added**

---

## 10. What's NOT in This Batch

- ❌ Sell item UI (sell is already in inventory, UI is separate)
- ❌ Gamble UI panel (just text messages for now)
- ❌ Multiple bet tiers
- ❌ 10-pull / pity system
- ❌ Gambler NPC dialogue
- ❌ Gambling animations
- ❌ Probability display
- ❌ Dynamic balancing

---

## 11. Review Gate Questions

1. **GAMBLER rewrite vs. new GAMBLE type** — Is replacing the existing GAMBLER behavior acceptable, or should we add a new enum value?
2. **Player::current_floor** — Is adding this field to Player acceptable for floor-dependent cost?
3. **Repeated interaction** — Is the `try_interact()` bypass approach clean enough, or should we add a `repeatable` flag to SpecialRoom?
4. **Reward pool weights (70/20/10)** — Are these reasonable starting values?
5. **Relic fallback** — When all relics are owned, give key as fallback. Acceptable?

---

*Design complete. Awaiting Review Gate before coding.*
