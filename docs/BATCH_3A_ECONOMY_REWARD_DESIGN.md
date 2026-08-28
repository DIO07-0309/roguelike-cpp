# Batch 3A — Economy + Reward Foundation Design

> **Status:** Design Phase — Awaiting Review Gate
> **Date:** 2026-08-28
> **Predecessor:** SPECIAL_ROOMS_EXPANSION_AUDIT.md
> **Scope:** Gold system, Key system, Unified Reward API, Relic persistence refactor

---

## 1. Design Goals

1. Create **Gold** as a per-run currency (Player field, save/load, HUD)
2. Create **Key** as a per-run consumable resource (Player field, save/load, HUD)
3. Create **unified Reward API** to replace 7+ copy-pasted reward paths
4. Refactor **RelicInstance.from_boss** → `PersistenceScope` enum
5. Ensure **backward-compatible save/load** (v3 → v4 with fallback)

## 2. Design Principles

- **Data-model first**: Gold and Key live on Player as `int` fields, not scattered in GameScene
- **Single source of truth**: One `RewardManager` handles all reward dispatch
- **Event consistency**: Every reward path emits the correct EventBus event
- **Save compatibility**: Old v3 saves load without data loss; new v4 format adds fields
- **No over-engineering**: Minimum viable abstraction; extend later

---

## 3. Gold System

### 3.1 Data Model

**File:** `src/game/entities/player.h`

```cpp
// 添加到 Player 类 (line ~50)
int gold = 0;  // 局内金币
```

**Why int, not a struct?** Gold is a single numeric resource. No complex behavior needed for MVP. A struct like `GoldManager` would be over-engineering for "add, subtract, query".

### 3.2 API

**File:** `src/game/entities/player.h` (add methods)

```cpp
// Gold API
void add_gold(int amount);
bool spend_gold(int amount);  // returns false if insufficient
int  get_gold() const;
```

**Implementation** (`player.cpp` or new `player_economy.cpp`):

```cpp
void Player::add_gold(int amount) {
    if (amount <= 0) return;
    gold += amount;
    EventBus::inst().emit(GameEventType::GOLD_GAIN, this, amount);
}

bool Player::spend_gold(int amount) {
    if (amount <= 0 || gold < amount) return false;
    gold -= amount;
    EventBus::inst().emit(GameEventType::GOLD_SPEND, this, amount);
    return true;
}

int Player::get_gold() const { return gold; }
```

### 3.3 Event Types

**File:** `src/game/core/event_types.h`

```cpp
// 添加到 GameEventType enum (after META_GAIN)
GOLD_GAIN,   // int_val = amount
GOLD_SPEND,  // int_val = amount
KEY_GAIN,    // int_val = amount
KEY_SPEND,   // int_val = amount
```

### 3.4 Save/Load

**File:** `src/game/save/save_manager.cpp`

**Save** (after `md:` line, ~line 67):
```cpp
fprintf(f, "gld:%d\n", player->gold);
fprintf(f, "key:%d\n", player->key_count);
```

**Load** (after `md:` parsing):
```cpp
else if (line.rfind("gld:", 0) == 0) {
    player->gold = std::atoi(line.c_str() + 4);
}
else if (line.rfind("key:", 0) == 0) {
    player->key_count = std::atoi(line.c_str() + 4);
}
```

**Backward compatibility:** If `gld:` or `key:` lines are missing (old v3 save), defaults to 0. No data loss.

### 3.5 HUD Display

**File:** `src/game/systems/game_renderer.cpp`

Add gold display to the existing HUD area (near HP bar or bottom-left):

```
💰 125  🔑 3
```

Use `g_font_small` for rendering. Position: bottom-left corner, above the hint bar.

### 3.6 Gold Sources (MVP)

| Source | Amount | Implementation |
|--------|--------|----------------|
| Monster kill | 5-15 gold | `on_monster_killed()` in `game_scene_combat.cpp` |
| Sell item | Rarity-based | `sell_item()` in Inventory (see §6) |
| Special rooms | Existing room rewards | Later (Batch 3B) |

### 3.7 Gold Sinks (MVP)

| Sink | Cost | Implementation |
|------|------|----------------|
| Gamble roll | 50 gold | Later (Batch 3B) |
| Future: Shop | TBD | Future |

---

## 4. Key System

### 4.1 Data Model

**File:** `src/game/entities/player.h`

```cpp
// 添加到 Player 类
int key_count = 0;  // 钥匙数量
```

**Why int (stacking) instead of KeyItem?** Keys are a resource, not an inventory item. They don't have rarity, don't take bag space, and are consumed on use. A simple counter is the right abstraction.

### 4.2 API

**File:** `src/game/entities/player.h`

```cpp
// Key API
void add_keys(int count);
bool consume_key();   // returns false if count == 0
int  get_key_count() const;
```

**Implementation:**

```cpp
void Player::add_keys(int count) {
    if (count <= 0) return;
    key_count += count;
    EventBus::inst().emit(GameEventType::KEY_GAIN, this, count);
}

bool Player::consume_key() {
    if (key_count <= 0) return false;
    key_count--;
    EventBus::inst().emit(GameEventType::KEY_SPEND, this, 1);
    return true;
}

int Player::get_key_count() const { return key_count; }
```

### 4.3 Key Sources (MVP)

| Source | Chance/Amount | Implementation |
|--------|--------------|----------------|
| Elite monster kill | 30% chance, 1 key | `on_monster_killed()` in `game_scene_combat.cpp` |
| Treasure room | 1 key (guaranteed) | `_exec_treasure()` in `special_room.cpp` |
| Future: Shop | Buy with gold | Future |

### 4.4 Key Sinks (MVP)

| Sink | Cost | Implementation |
|------|------|----------------|
| Challenge Room gate | 1 key | Batch 4 |

---

## 5. Unified Reward API

### 5.1 Problem Statement

Currently, 10+ code paths grant items/relics to the player, each with its own logic:

| Source | Item Grant | Relic Grant | Event Emitted |
|--------|-----------|-------------|---------------|
| Boss kill | `_drop_boss_reward()` | `player->relics.push_back({id, true})` | RELIC_GAIN ✅ |
| Monster loot | `ground_items.push_back()` | — | None ❌ |
| Treasure room | `inventory.add()` | `_try_grant_random_relic()` | None ❌ |
| Shop | `inventory.add()` | — | None ❌ |
| Events (7 types) | `inventory.add()` | `player->relics.push_back({id})` | None ❌ |
| Quests | — | `player->relics.push_back({id})` | None ❌ |
| Relic events | — | `player->relics.push_back({id})` | None ❌ |

**Issues:**
- Copy-pasted relic granting logic in 4+ places
- RELIC_GAIN event only emitted from boss kill (1 of 10 paths)
- ITEM_PICKUP event defined but never emitted
- No consistent notification/UI hook

### 5.2 Solution: RewardManager

**New file:** `src/game/systems/reward_manager.h`

```cpp
#pragma once
#include <string>
#include <memory>

class Player;
struct Item;
enum class Rarity;

// ============================================================
// RewardSource — 奖励来源标记 (用于事件/统计/UI)
// ============================================================
enum class RewardSource {
    BOSS_KILL,
    MONSTER_DROP,
    TREASURE_CHEST,
    SPECIAL_ROOM,
    EVENT,
    QUEST,
    GAMBLE,
    CHALLENGE,
    SHOP,
};

// ============================================================
// RewardManager — 统一奖励发放 API
// ============================================================
class RewardManager {
public:
    // 发放金币
    static void grant_gold(Player* player, int amount, RewardSource source);

    // 发放钥匙
    static void grant_keys(Player* player, int count, RewardSource source);

    // 发放装备/消耗品 → 进入背包
    static bool grant_item(Player* player, std::shared_ptr<Item> item, RewardSource source);

    // 发放装备/消耗品 → 掉落至地面 (返回 DroppedItem)
    // 调用方自行决定放置位置

    // 发放遗物 (带 PersistenceScope)
    static bool grant_relic(Player* player, const std::string& relic_id,
                            RewardSource source, PersistenceScope scope);

    // 发放随机遗物 (rarity-weighted + build affinity)
    static std::string grant_random_relic(Player* player, float drop_chance,
                                          RewardSource source, PersistenceScope scope);
};
```

### 5.3 Implementation

**New file:** `src/game/systems/reward_manager.cpp`

```cpp
#include "reward_manager.h"
#include "../entities/player.h"
#include "../core/event_bus.h"
#include "../world/special_room.h"  // for _try_grant_random_relic logic

void RewardManager::grant_gold(Player* p, int amount, RewardSource src) {
    if (!p || amount <= 0) return;
    p->add_gold(amount);
    // Event already emitted by Player::add_gold
}

void RewardManager::grant_keys(Player* p, int count, RewardSource src) {
    if (!p || count <= 0) return;
    p->add_keys(count);
    // Event already emitted by Player::add_keys
}

bool RewardManager::grant_item(Player* p, std::shared_ptr<Item> item, RewardSource src) {
    if (!p || !item) return false;
    bool ok = p->inventory.add(item, p);
    if (ok) {
        EventBus::inst().emit(GameEventType::ITEM_PICKUP, p,
                              static_cast<int>(src));
    }
    return ok;
}

bool RewardManager::grant_relic(Player* p, const std::string& id,
                                RewardSource src, PersistenceScope scope) {
    if (!p || id.empty()) return false;
    if (player_has_relic(p, id)) return false;  // already owned

    p->relics.push_back({id, scope});
    const RelicDef* def = get_relic_def(id);
    if (def) {
        g_relic_archive.mark_obtained(id, rarity_level(def->rarity));
        EventBus::inst().emit(GameEventType::RELIC_GAIN, p,
                              rarity_level(def->rarity),
                              0.0f, id.c_str());
    }
    return true;
}
```

### 5.4 Refactor: RelicInstance PersistenceScope

**File:** `src/game/entities/combat_stats.h`

**Before:**
```cpp
struct RelicInstance {
    std::string id;
    bool from_boss = false;
};
```

**After:**
```cpp
enum class PersistenceScope : uint8_t {
    FLOOR = 0,    // 当前楼层有效, 换层消失 (普通遗物)
    RUN = 1,      // 整局有效, 换层保留 (Boss/Gamble 遗物)
    PERMANENT = 2 // 永久保留 (未来: Meta 解锁)
};

struct RelicInstance {
    std::string id;
    PersistenceScope scope = PersistenceScope::FLOOR;
};
```

**Migration:** `from_boss=true` → `scope=RUN`. `from_boss=false` → `scope=FLOOR`.

### 5.5 Floor Transition Logic

**File:** `src/game/scenes/game_scene.cpp` (lines 289-293)

**Before:**
```cpp
std::vector<RelicInstance> keep;
for (auto& r : player->relics)
    if (r.from_boss) keep.push_back(r);
player->relics = keep;
```

**After:**
```cpp
player->relics.erase(
    std::remove_if(player->relics.begin(), player->relics.end(),
        [](const RelicInstance& r) {
            return r.scope == PersistenceScope::FLOOR;
        }),
    player->relics.end());
```

### 5.6 Save/Load for Relics (New in v4)

**Current state:** Relics are NOT saved at all (B13 decision).

**Problem:** Boss relics (`from_boss=true`) are lost on save+load. This is a data integrity gap.

**Solution:** Save relics with `PersistenceScope` in v4 format.

**Save:**
```cpp
fprintf(f, "rlc:");
for (auto& r : player->relics) {
    fprintf(f, "%s,%d;", r.id.c_str(), static_cast<int>(r.scope));
}
fprintf(f, "\n");
```

**Load:**
```cpp
else if (line.rfind("rlc:", 0) == 0) {
    // Parse "relic_id,scope;relic_id,scope;..."
    // Skip if scope == FLOOR (already cleared by floor transition)
    // Restore if scope == RUN or PERMANENT
}
```

**Backward compatibility:** Old v3 saves have no `rlc:` line → player gets 0 relics (same as current behavior). Old `rlc:` lines (if any残留) are silently ignored.

---

## 6. Item Sell Mechanic

### 6.1 API

**File:** `src/game/entities/inventory.h`

```cpp
// 新增方法
int sell_item(int index, Player* player);  // Returns gold gained, 0 if invalid
```

**Implementation** (`inventory.cpp`):

```cpp
int Inventory::sell_item(int index, Player* player) {
    if (index < 0 || index >= (int)items.size()) return 0;
    auto& item = items[index];
    int value = _calc_sell_value(item.get());
    items.erase(items.begin() + index);
    if (player) player->add_gold(value);
    return value;
}
```

### 6.2 Sell Value Calculation

**File:** `src/game/entities/item.cpp` (new helper)

```cpp
int calc_sell_value(const Item* item) {
    if (!item) return 0;
    int base = 0;
    if (auto* eq = dynamic_cast<const EquipmentItem*>(item)) {
        base = 10 + static_cast<int>(eq->rarity) * 8;
    } else if (auto* cn = dynamic_cast<const ConsumableItem*>(item)) {
        base = 5 + static_cast<int>(cn->rarity) * 4;
    }
    return base;  // COMMON=10, RARE=18, EPIC=26, LEGENDARY=34 (equipment)
}
```

**Sell values (MVP):**

| Rarity | Equipment | Consumable |
|--------|-----------|------------|
| COMMON | 10 gold | 5 gold |
| RARE | 18 gold | 9 gold |
| EPIC | 26 gold | 13 gold |
| LEGENDARY | 34 gold | 17 gold |

---

## 7. File Change Summary

### New Files (2)

| File | Purpose | Est. Lines |
|------|---------|-----------|
| `src/game/systems/reward_manager.h` | Unified reward API header | ~40 |
| `src/game/systems/reward_manager.cpp` | Unified reward API implementation | ~80 |

### Modified Files (8)

| File | Changes | Est. Lines Changed |
|------|---------|-------------------|
| `src/game/entities/player.h` | Add `gold`, `key_count`, Gold/Key API methods | +8 |
| `src/game/entities/player.cpp` | Implement Gold/Key methods | +20 |
| `src/game/entities/combat_stats.h` | `RelicInstance`: `from_boss` → `PersistenceScope scope` | ~5 |
| `src/game/entities/item.h` | Add `calc_sell_value()` declaration | +1 |
| `src/game/entities/item.cpp` | Implement `calc_sell_value()` | +10 |
| `src/game/entities/inventory.h` | Add `sell_item()` method | +1 |
| `src/game/entities/inventory.cpp` | Implement `sell_item()` | +10 |
| `src/game/core/event_types.h` | Add GOLD_GAIN, GOLD_SPEND, KEY_GAIN, KEY_SPEND events | +4 |
| `src/game/save/save_manager.cpp` | Save/load `gld:`, `key:`, `rlc:` lines (v4 format) | +30 |
| `src/game/scenes/game_scene.cpp` | Floor transition: use `PersistenceScope` instead of `from_boss` | ~3 |
| `src/game/systems/game_renderer.cpp` | HUD: gold + key count display | +15 |
| `src/game/scene/game_scene_combat.cpp` | Boss kill: use `RewardManager::grant_relic()` | ~5 |
| `src/game/world/special_room.cpp` | Treasure/Shrine/Secret: use `RewardManager::grant_relic()` | ~10 |
| `src/game/world/event_system.cpp` | Events: use `RewardManager::grant_relic()` | ~15 |
| `src/game/world/quest_manager.cpp` | Quest rewards: use `RewardManager::grant_relic()` | ~5 |

### Test Files (2)

| File | Purpose | Est. Lines |
|------|---------|-----------|
| `tests/economy/gold_test.cpp` | Gold add/spend/save/load | ~40 |
| `tests/economy/key_test.cpp` | Key add/consume/save/load | ~40 |

**Total estimated new/changed code: ~300 lines**

---

## 8. Save Format v4 Specification

```
v:4                          ← version bump
floor:5
maxf:10
lv:8
xp:120
xpt:200
mhp:150
chp:120
atk:18
pd:8
md:5
gld:250                      ← NEW: gold
key:3                        ← NEW: key count
act:slash,3,1,15;fireball,2,0,8;
pas:iron_skin,2,1,5;
inv:Iron Sword,1,weapon,12,5,0;生命药水,0,heal,25,;
eqw:Iron Sword,1,weapon,12,5,0
wpn:sword_rare
eqa:Leather Armor,0,armor,0,4,2
buf:attack_up,2,5.30,1.00;
seed:1234567890
spr:1,0,1,0,1,0,0,1,0
spd:0,0,0,0,0,0,0,0,0
atl:3
rul:combo_hit=15;boss_damage=200;
qst:1=2;3=1;
elem:1,2,150,1
end:1;2;
rlc:blood_charm,1;venom_fang,0;     ← NEW: relics with scope
mra:0.1,0.2,0.3,...
mrb:0.4,0.5,0.6,...
```

**Key format changes:**
- `v:3` → `v:4`
- Added `gld:` line
- Added `key:` line
- Re-enabled `rlc:` line with scope field

---

## 9. Backward Compatibility Matrix

| Old Save (v3) | New Code Behavior |
|---------------|-------------------|
| No `gld:` line | `player->gold = 0` (default) |
| No `key:` line | `player->key_count = 0` (default) |
| No `rlc:` line | `player->relics` empty (same as before) |
| Old `rlc:` line (residual) | Silently ignored (same as before) |

| New Save (v4) | Old Code Behavior |
|---------------|-------------------|
| Has `gld:` line | Unknown line prefix → ignored (safe) |
| Has `key:` line | Unknown line prefix → ignored (safe) |
| Has `rlc:` with scope | Old code ignores `rlc:` entirely (B13) |

**Both directions are safe.** No data corruption on version mismatch.

---

## 10. Implementation Order

| Step | Task | Files | Dependencies |
|------|------|-------|-------------|
| 1 | Add GOLD/KEY event types | event_types.h | None |
| 2 | Add `gold`, `key_count` to Player | player.h, player.cpp | Step 1 |
| 3 | Save/load gold + key | save_manager.cpp | Step 2 |
| 4 | HUD display | game_renderer.cpp | Step 2 |
| 5 | `sell_item()` + `calc_sell_value()` | inventory.h/cpp, item.h/cpp | Step 2 |
| 6 | `PersistenceScope` refactor | combat_stats.h, game_scene.cpp | None |
| 7 | Save/load relics with scope | save_manager.cpp | Step 6 |
| 8 | Create RewardManager | reward_manager.h/cpp | Steps 1-7 |
| 9 | Migrate boss kill to RewardManager | game_scene_combat.cpp | Step 8 |
| 10 | Migrate special rooms to RewardManager | special_room.cpp | Step 8 |
| 11 | Migrate events to RewardManager | event_system.cpp | Step 8 |
| 12 | Migrate quests to RewardManager | quest_manager.cpp | Step 8 |
| 13 | Monster kill gold + key drop | game_scene_combat.cpp | Steps 2, 8 |
| 14 | Tests | tests/economy/ | Steps 1-13 |

---

## 11. Review Gate Questions

1. **Gold as `int` on Player** — Is this sufficient, or should it be a separate `EconomyComponent`?
2. **Key as `int` on Player** — Same question. Counter vs. component?
3. **PersistenceScope enum** — Is FLOOR/RUN/PERMANENT the right set? Any other scopes needed?
4. **Relic save/load in v4** — Is this the right time to re-enable relic persistence, or defer?
5. **Sell values** — Are the proposed sell values (10/18/26/34 gold) reasonable as starting points?
6. **RewardManager scope** — Should it handle ground item drops too, or just inventory grants?
7. **HUD placement** — Bottom-left for gold/key, or somewhere else?
8. **Gold from monsters** — Fixed 5-15, or scale with floor?

---

*Design complete. Awaiting Review Gate before coding.*
