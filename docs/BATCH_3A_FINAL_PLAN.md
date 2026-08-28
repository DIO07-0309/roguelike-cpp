# Batch 3A — Economy & Persistence Foundation (Final Plan)

> **Status:** ✅ Review Gate Approved — Ready to Code
> **Date:** 2026-08-28
> **Predecessor:** BATCH_3A_ECONOMY_REWARD_DESIGN.md + Review Gate 裁决

---

## Scope

**做：** Gold / Key 资源模型 + PersistenceScope + Save v4 + 最小 RewardManager + 测试
**不做：** Gamble Room / Challenge Room / 怪物掉金币 / Gold Scale / 波次生成 / 抽奖概率

---

## Review Gate 裁决汇总

| 问题 | 决策 |
|------|------|
| Q1 Gold 存储 | 🟢 `int gold` on Player |
| Q2 Key 存储 | 🟢 `int key_count` on Player, API: `add_key/spend_key` |
| Q3 PersistenceScope | 🟢 `{ FLOOR, RUN }` 二态 |
| Q4 Save Relic | 🟢 只保存 RUN relics, `rlc:id,scope;` 格式 |
| Q5 Sell Price | 🟢 固定值 (10/18/26/34), 统一 `get_sell_value()` |
| Q6 RewardManager | 🟡 最小职责: grant_item/grant_gold/grant_key/grant_relic |
| Q7 HUD | 🟢 底部左侧, 文字标签 `G: 125  K: 3`, 不用 Emoji |
| Q8 Gold Source | 🔴 暂不做怪物掉金币, Gold 仅来自装备出售 |

---

## 1. Player — Gold & Key

### File: `src/game/entities/player.h`

在 `int xp_to_next = 80;` (line 52) 之后添加:

```cpp
int gold = 0;        // 局内金币 (Batch 3A)
int key_count = 0;   // 钥匙数量 (Batch 3A)
```

在 `void auto_level_to(int target);` (line 72) 之后添加:

```cpp
// Batch 3A: 经济 API
void add_gold(int amount);
bool spend_gold(int amount);
int  get_gold() const;

void add_key(int amount);
bool spend_key(int amount);
int  get_key_count() const;
```

### File: `src/game/entities/player.cpp`

添加 6 个方法实现 (~24 行):

```cpp
void Player::add_gold(int amount) {
    if (amount <= 0) return;
    gold += amount;
}

bool Player::spend_gold(int amount) {
    if (amount <= 0 || gold < amount) return false;
    gold -= amount;
    return true;
}

int Player::get_gold() const { return gold; }

void Player::add_key(int amount) {
    if (amount <= 0) return;
    key_count += amount;
}

bool Player::spend_key(int amount) {
    if (amount <= 0 || key_count < amount) return false;
    key_count -= amount;
    return true;
}

int Player::get_key_count() const { return key_count; }
```

**注意：** 不在 add_gold/add_key 中 emit event。Event 在 Batch 3B (Gamble Room) 需要时再加。当前 MVP 只需数据模型正确。

---

## 2. PersistenceScope

### File: `src/game/entities/combat_stats.h`

在 `RelicInstance` struct (line 26-29) 处修改:

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
    FLOOR = 0,  // 当前楼层有效, 换层消失
    RUN = 1,    // 整局有效, 换层保留
};

struct RelicInstance {
    std::string id;
    PersistenceScope scope = PersistenceScope::FLOOR;
};
```

### File: `src/game/scenes/game_scene.cpp` (line 289-293)

**Before:**
```cpp
// G10: Boss relics persist across floors (from_boss=true)
std::vector<RelicInstance> keep;
for (auto& r : player->relics)
    if (r.from_boss) keep.push_back(r);
player->relics = keep;
```

**After:**
```cpp
// Batch 3A: Relics with PersistenceScope::FLOOR are removed on floor transition
player->relics.erase(
    std::remove_if(player->relics.begin(), player->relics.end(),
        [](const RelicInstance& r) {
            return r.scope == PersistenceScope::FLOOR;
        }),
    player->relics.end());
```

### File: `src/game/scene/game_scene_combat.cpp` (line 138)

**Before:**
```cpp
_s.player->relics.push_back({chosen, true});  // G10: boss relic persists
```

**After:**
```cpp
_s.player->relics.push_back({chosen, PersistenceScope::RUN});
```

### Other `from_boss` references (grep results)

需要修改所有 `from_boss` 引用:

| File | Line | Change |
|------|------|--------|
| `game_scene_combat.cpp` | 138 | `{chosen, true}` → `{chosen, PersistenceScope::RUN}` |
| `game_scene.cpp` | 289-293 | 替换为 `remove_if` 逻辑 |
| `special_room.cpp` | 183 | `{chosen}` → `{chosen, PersistenceScope::FLOOR}` (显式标注) |
| `event_system.cpp` | 279, 299, 337, 367, 463, 475, 500 | `{id}` → `{id, PersistenceScope::FLOOR}` |
| `quest_manager.cpp` | 148 | `{r.relic_id}` → `{r.relic_id, PersistenceScope::FLOOR}` |

**总计 ~10 处修改，全部是替换字面量。**

---

## 3. Save v4

### File: `src/game/save/save_manager.cpp`

#### Save (在 `fprintf(f, "md:%d\n", ...)` 之后, ~line 67)

```cpp
fprintf(f, "gld:%d\n", player->gold);
fprintf(f, "key:%d\n", player->key_count);
```

#### Save relics (在 `spr:` 行之后, ~line 140)

```cpp
// Batch 3A: 保存 RUN relics
fprintf(f, "rlc:");
for (auto& r : player->relics) {
    if (r.scope == PersistenceScope::RUN)
        fprintf(f, "%s,%d;", r.id.c_str(), static_cast<int>(r.scope));
}
fprintf(f, "\n");
```

#### Version bump (line 57)

**Before:** `fprintf(f, "v:3\n");`
**After:** `fprintf(f, "v:4\n");`

#### Load — parse gold + key (在 `md:` 解析之后, ~line 255)

```cpp
int gld   = getV("gld", 0);
int kcnt  = getV("key", 0);
```

#### Load — apply to player (在 `p->xp_to_next = xpt;` 之后, ~line 262)

```cpp
p->gold = gld;
p->key_count = kcnt;
```

#### Load — parse relics (替换 line 250 的 B13 注释)

```cpp
// Batch 3A: 读取 RUN relics (v4+)
std::string rlc_str = getS("rlc");
if (!rlc_str.empty()) {
    for (size_t pos = 0; pos < rlc_str.size(); ) {
        size_t semi = rlc_str.find(';', pos);
        std::string tok = rlc_str.substr(pos, (semi != std::string::npos ? semi - pos : std::string::npos));
        if (tok.empty()) break;
        size_t comma = tok.find(',');
        if (comma != std::string::npos) {
            std::string id = tok.substr(0, comma);
            int scope_val = std::atoi(tok.substr(comma + 1).c_str());
            if (scope_val == static_cast<int>(PersistenceScope::RUN) && !id.empty()) {
                // 验证 relic_id 是否有效
                const RelicDef* def = get_relic_def(id);
                if (def) player->relics.push_back({id, PersistenceScope::RUN});
            }
        }
        pos = (semi != std::string::npos) ? semi + 1 : std::string::npos;
    }
}
```

**注意：** 需要 `#include "../systems/combat_system.h"` (for `get_relic_def`) 和 `#include "../entities/combat_stats.h"` (for `RelicInstance`, `PersistenceScope`)。

#### Backward compatibility

| Old save (v3) | New code behavior |
|---------------|-------------------|
| No `gld:` line | `gold = 0` (default via getV) |
| No `key:` line | `key_count = 0` (default via getV) |
| No `rlc:` line | relics empty (same as before) |
| Old `rlc:` line (residual) | Parsed but scope=0=FLOOR → skipped |

---

## 4. HUD Display

### File: `src/game/systems/game_renderer.cpp`

在 `draw_hud()` 函数中，Key hints 之前 (~line 774)，添加:

```cpp
// Batch 3A: Gold / Key HUD
if (g_font_loaded && player) {
    char buf[32];
    snprintf(buf, sizeof(buf), "G: %d  K: %d", player->gold, player->key_count);
    DrawTextEx(g_font_small, buf,
               {14.0f, (float)screen_h - 26.0f},
               12, 1, Color{220, 200, 100, 220});
}
```

位置: 底部左侧, 与右侧 `[R]圣物...` 提示对称。

---

## 5. Sell Item

### File: `src/game/entities/item.h`

在 `generate_random_charm()` 声明之后 (~line 96) 添加:

```cpp
int get_sell_value(const Item* item);
```

### File: `src/game/entities/item.cpp`

添加 (~10 行):

```cpp
int get_sell_value(const Item* item) {
    if (!item) return 0;
    int rarity_idx = static_cast<int>(item->rarity);
    if (auto* eq = dynamic_cast<const EquipmentItem*>(item)) {
        // Equipment: 10, 18, 26, 34
        return 10 + rarity_idx * 8;
    }
    if (auto* cn = dynamic_cast<const ConsumableItem*>(item)) {
        // Consumable: 5, 9, 13, 17
        return 5 + rarity_idx * 4;
    }
    return 0;
}
```

### File: `src/game/entities/inventory.h`

添加方法声明:

```cpp
int sell_item(int index, Player* player);
```

### File: `src/game/entities/inventory.cpp`

添加 (~10 行):

```cpp
#include "item.h"  // for get_sell_value

int Inventory::sell_item(int index, Player* player) {
    if (index < 0 || index >= (int)items.size()) return 0;
    int value = get_sell_value(items[index].get());
    items.erase(items.begin() + index);
    if (player) player->add_gold(value);
    return value;
}
```

---

## 6. RewardManager (最小版)

### New file: `src/game/systems/reward_manager.h`

```cpp
#pragma once
#include <memory>
#include <string>

class Player;
struct Item;
enum class PersistenceScope : uint8_t;

// ============================================================
// Batch 3A: RewardManager — 最小奖励发放层
// 职责: 安全地将奖励写入 Player, 不负责概率/生成/UI
// ============================================================
class RewardManager {
public:
    static bool grant_item(Player& player, std::shared_ptr<Item> item);
    static void grant_gold(Player& player, int amount);
    static void grant_key(Player& player, int count);
    static bool grant_relic(Player& player, const std::string& relic_id,
                            PersistenceScope scope);
};
```

### New file: `src/game/systems/reward_manager.cpp`

```cpp
#include "reward_manager.h"
#include "../entities/player.h"
#include "../entities/combat_stats.h"  // RelicInstance, PersistenceScope
#include "../systems/combat_system.h"  // get_relic_def, player_has_relic, g_relic_archive

bool RewardManager::grant_item(Player& player, std::shared_ptr<Item> item) {
    if (!item) return false;
    return player.inventory.add(item, &player);
}

void RewardManager::grant_gold(Player& player, int amount) {
    player.add_gold(amount);
}

void RewardManager::grant_key(Player& player, int count) {
    player.add_key(count);
}

bool RewardManager::grant_relic(Player& player, const std::string& relic_id,
                                PersistenceScope scope) {
    if (relic_id.empty()) return false;
    if (player_has_relic(&player, relic_id)) return false;
    player.relics.push_back({relic_id, scope});
    const RelicDef* def = get_relic_def(relic_id);
    if (def) g_relic_archive.mark_obtained(relic_id, rarity_level(def->rarity));
    return true;
}
```

**职责边界：**
- ✅ 安全写入 Player 字段
- ✅ 检查重复 (relic)
- ✅ 记录 archive (relic)
- ❌ 不管概率
- ❌ 不管随机生成
- ❌ 不管地面掉落
- ❌ 不管 UI/通知

---

## 7. Migrate Boss Kill to RewardManager

### File: `src/game/scene/game_scene_combat.cpp` (lines 130-147)

**Before:**
```cpp
// Boss relic
auto all_ids = get_all_relic_ids();
std::vector<std::string> candidates;
for (auto& id : all_ids)
    if (!player_has_relic(_s.player.get(), id))
        candidates.push_back(id);
if (!candidates.empty()) {
    std::string chosen = candidates[rng() % candidates.size()];
    _s.player->relics.push_back({chosen, true});
    const RelicDef* def = get_relic_def(chosen);
    if (def) {
        _s._presentation.room_msg = "RELIC:" + def->name;
        g_relic_archive.mark_obtained(chosen, rarity_level(def->rarity));
        EventBus::inst().emit(GameEventType::RELIC_GAIN, ...);
    }
}
```

**After:**
```cpp
// Boss relic — via RewardManager
{
    auto all_ids = get_all_relic_ids();
    std::vector<std::string> candidates;
    for (auto& id : all_ids)
        if (!player_has_relic(_s.player.get(), id))
            candidates.push_back(id);
    if (!candidates.empty()) {
        std::string chosen = candidates[rng() % candidates.size()];
        RewardManager::grant_relic(*_s.player, chosen, PersistenceScope::RUN);
        const RelicDef* def = get_relic_def(chosen);
        if (def) {
            _s._presentation.room_msg = "RELIC:" + def->name;
            EventBus::inst().emit(GameEventType::RELIC_GAIN, _s.player.get(),
                                   rarity_level(def->rarity), 0.0f, chosen.c_str());
        }
        _s._presentation.room_msg_timer = 3.5f;
    }
}
```

**注意：** 保留 `EventBus::emit` 和 `room_msg` 逻辑 — 这些是 UI/通知层，不属于 RewardManager 职责。

---

## 8. Tests

### New file: `tests/economy/gold_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "entities/player.h"

TEST(GoldTest, InitialZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.get_gold(), 0);
}

TEST(GoldTest, AddGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(50);
    EXPECT_EQ(p.get_gold(), 50);
    p.add_gold(30);
    EXPECT_EQ(p.get_gold(), 80);
}

TEST(GoldTest, SpendGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(100);
    EXPECT_TRUE(p.spend_gold(40));
    EXPECT_EQ(p.get_gold(), 60);
}

TEST(GoldTest, SpendInsufficient) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(10);
    EXPECT_FALSE(p.spend_gold(50));
    EXPECT_EQ(p.get_gold(), 10);  // unchanged
}

TEST(GoldTest, AddNegativeNoop) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_gold(-10);
    EXPECT_EQ(p.get_gold(), 0);
}
```

### New file: `tests/economy/key_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "entities/player.h"

TEST(KeyTest, InitialZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.get_key_count(), 0);
}

TEST(KeyTest, AddKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(3);
    EXPECT_EQ(p.get_key_count(), 3);
}

TEST(KeyTest, SpendKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(2);
    EXPECT_TRUE(p.spend_key(1));
    EXPECT_EQ(p.get_key_count(), 1);
}

TEST(KeyTest, SpendInsufficient) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    p.add_key(1);
    EXPECT_FALSE(p.spend_key(2));
    EXPECT_EQ(p.get_key_count(), 1);
}
```

### New file: `tests/economy/relic_persistence_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "entities/combat_stats.h"

TEST(PersistenceScopeTest, FloorDefault) {
    RelicInstance r{"test_id"};
    EXPECT_EQ(r.scope, PersistenceScope::FLOOR);
}

TEST(PersistenceScopeTest, RunScope) {
    RelicInstance r{"boss_relic", PersistenceScope::RUN};
    EXPECT_EQ(r.scope, PersistenceScope::RUN);
}

TEST(PersistenceScopeTest, FloorScopeExplicit) {
    RelicInstance r{"temp_relic", PersistenceScope::FLOOR};
    EXPECT_EQ(r.scope, PersistenceScope::FLOOR);
}
```

### New file: `tests/economy/save_v4_test.cpp`

需要集成测试: save → load → verify gold/key/relics. 但需要 mock 或 headless 环境。

**MVP 简化：** 先写 unit test 验证 save format string 生成正确，集成测试在 Batch 3B 补充。

### New file: `tests/economy/reward_manager_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "entities/player.h"
#include "systems/reward_manager.h"

TEST(RewardManagerTest, GrantGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    RewardManager::grant_gold(p, 100);
    EXPECT_EQ(p.get_gold(), 100);
}

TEST(RewardManagerTest, GrantKey) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    RewardManager::grant_key(p, 2);
    EXPECT_EQ(p.get_key_count(), 2);
}

TEST(RewardManagerTest, GrantItemNull) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_FALSE(RewardManager::grant_item(p, nullptr));
}
```

---

## 9. CMakeLists.txt

### File: `tests/CMakeLists.txt`

添加 5 个新测试:

```cpp
add_executable(gold_test economy/gold_test.cpp)
target_link_libraries(gold_test PRIVATE game_lib GTest::gtest_main)
add_test(NAME gold_test COMMAND gold_test)

add_executable(key_test economy/key_test.cpp)
target_link_libraries(key_test PRIVATE game_lib GTest::gtest_main)
add_test(NAME key_test COMMAND key_test)

add_executable(relic_persistence_test economy/relic_persistence_test.cpp)
target_link_libraries(relic_persistence_test PRIVATE game_lib GTest::gtest_main)
add_test(NAME relic_persistence_test COMMAND relic_persistence_test)

add_executable(save_v4_test economy/save_v4_test.cpp)
target_link_libraries(save_v4_test PRIVATE game_lib GTest::gtest_main)
add_test(NAME save_v4_test COMMAND save_v4_test)

add_executable(reward_manager_test economy/reward_manager_test.cpp)
target_link_libraries(reward_manager_test PRIVATE game_lib GTest::gtest_main)
add_test(NAME reward_manager_test COMMAND reward_manager_test)
```

**注意：** 需要创建 `tests/economy/` 目录。

---

## 10. File Change Summary

### New Files (7)

| File | Purpose | Est. Lines |
|------|---------|-----------|
| `src/game/systems/reward_manager.h` | 最小 Reward API | ~20 |
| `src/game/systems/reward_manager.cpp` | 最小 Reward 实现 | ~35 |
| `tests/economy/gold_test.cpp` | Gold unit tests | ~35 |
| `tests/economy/key_test.cpp` | Key unit tests | ~30 |
| `tests/economy/relic_persistence_test.cpp` | PersistenceScope tests | ~20 |
| `tests/economy/save_v4_test.cpp` | Save v4 format tests | ~40 |
| `tests/economy/reward_manager_test.cpp` | RewardManager tests | ~25 |

### Modified Files (9)

| File | Changes | Lines Changed |
|------|---------|--------------|
| `src/game/entities/player.h` | +gold, +key_count, +6 methods | +8 |
| `src/game/entities/player.cpp` | +6 method implementations | +24 |
| `src/game/entities/combat_stats.h` | PersistenceScope enum, RelicInstance.scope | ~5 |
| `src/game/entities/item.h` | +get_sell_value() declaration | +1 |
| `src/game/entities/item.cpp` | +get_sell_value() implementation | +10 |
| `src/game/entities/inventory.h` | +sell_item() declaration | +1 |
| `src/game/entities/inventory.cpp` | +sell_item() implementation | +10 |
| `src/game/save/save_manager.cpp` | v4 format, gld/key/rlc save/load | +35 |
| `src/game/scenes/game_scene.cpp` | Floor transition: use PersistenceScope | ~3 |
| `src/game/scene/game_scene_combat.cpp` | Boss kill: use RewardManager + PersistenceScope | ~8 |
| `src/game/systems/game_renderer.cpp` | Gold/Key HUD display | +8 |
| `src/game/world/special_room.cpp` | Explicit FLOOR scope on relic grants | ~3 |
| `src/game/world/event_system.cpp` | Explicit FLOOR scope on relic grants | ~7 |
| `src/game/world/quest_manager.cpp` | Explicit FLOOR scope on relic grant | ~1 |
| `tests/CMakeLists.txt` | +5 test registrations | +15 |

**Total estimated: ~195 lines new/changed code**

---

## 11. Implementation Order

| Step | Task | Files |
|------|------|-------|
| 1 | PersistenceScope enum | combat_stats.h |
| 2 | Player gold + key fields + API | player.h, player.cpp |
| 3 | get_sell_value() | item.h, item.cpp |
| 4 | sell_item() | inventory.h, inventory.cpp |
| 5 | RewardManager | reward_manager.h, reward_manager.cpp |
| 6 | Save v4 (gold, key, relics) | save_manager.cpp |
| 7 | Floor transition | game_scene.cpp |
| 8 | Boss kill migration | game_scene_combat.cpp |
| 9 | Relic grants: explicit FLOOR scope | special_room.cpp, event_system.cpp, quest_manager.cpp |
| 10 | HUD display | game_renderer.cpp |
| 11 | Tests | tests/economy/* |
| 12 | CMakeLists.txt | tests/CMakeLists.txt |
| 13 | Compile + ctest + desktop sync | — |

---

## 12. Verification Checklist

- [ ] `cmake --build build --config Release` — 编译通过
- [ ] `ctest --test-dir build` — 全部通过 (44 existing + 5 new = 49)
- [ ] 新存档: save → load → gold/key/relics 正确恢复
- [ ] 旧 v3 存档: load → gold=0, key=0, relics=空 (兼容)
- [ ] HUD 显示: `G: 0  K: 0` (初始状态)
- [ ] Boss 击杀: RUN relic 正确授予
- [ ] 楼层切换: FLOOR relic 消失, RUN relic 保留
- [ ] 装备出售: 获得对应 gold
- [ ] 桌面版同步

---

*Plan complete. Ready to code.*
