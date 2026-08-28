# Batch 3C — Inventory Sell UI Implementation Plan

> **Status:** Design Phase — Awaiting Review Gate
> **Date:** 2026-08-28
> **Predecessor:** Batch 3B (Gamble Room MVP)
> **Scope:** Inventory Sell UI — close the gold economy loop

---

## 1. Goal

```
[B] 打开背包 → 选中装备/消耗品 → [T] 出售 → 获得 Gold → UI 刷新
```

Complete the loop: 打怪 → 获得装备 → 出售 → Gold → Gamble Room

---

## 2. Key Finding: Everything Exists, Just Wire It

| Component | Status | File |
|-----------|--------|------|
| `sell_item(index, player)` | ✅ 已实现 | `inventory.cpp:68-74` |
| `get_sell_value(item)` | ✅ 已实现 | `item.cpp:279-291` |
| `Player::add_gold()` | ✅ 已实现 | `player.cpp` |
| `RewardManager::grant_gold()` | ✅ 已实现 | `reward_manager.cpp` |
| Inventory UI rendering | ✅ 已实现 | `game_renderer.cpp:464-526` |
| Inventory input handling | ✅ 已实现 | `player_controller.cpp:199-224` |
| **Sell key binding** | ❌ **未接入** | 需添加 |
| **Sell hint in UI** | ❌ **未显示** | 需修改 |

**Batch 3C = 100% UI 接入，0% 新逻辑。**

---

## 3. Key Binding Decision

**[T] = Sell** (用户选择)

现有 inventory 按键：
| Key | Action |
|-----|--------|
| B/ESC | Close |
| X | Equip |
| U | Use |
| D | Drop |
| W/S/↑/↓ | Navigate |
| ←/→ | Page |
| **T** | **Sell (NEW)** |

无冲突。

---

## 4. File Changes

### 4.1 `player_controller.cpp` — Add [T] sell handler

**Location:** Inside the `if (gs.inventory_open)` block (line 199-224)

**After the [D] drop handler (line ~214), add:**

```cpp
else if (IsKeyPressed(KEY_T)) {
    int val = gs.player->inventory.sell_item(gs.inventory_cursor, gs.player.get());
    if (val > 0) {
        gs._presentation.room_msg = "出售获得 " + std::to_string(val) + " Gold";
        gs._presentation.room_msg_timer = 2.0f;
    }
    int item_count = (int)gs.player->inventory.items.size();
    gs.inventory_cursor = std::min(gs.inventory_cursor, std::max(0, item_count - 1));
}
```

**Impact:** ~8 lines added. No existing code modified.

### 4.2 `game_renderer.cpp` — Update key hints

**Location:** Line 523-524, the key hints bar

**Before:**
```cpp
"^v选择 X装备 U使用 D丢弃 B关闭"
```

**After:**
```cpp
"^v选择 X装备 T出售 U使用 D丢弃 B关闭"
```

**Impact:** 1 line changed.

---

## 5. Total Change Summary

| File | Change | Est. Lines |
|------|--------|-----------|
| `player_controller.cpp` | Add [T] sell handler | +8 |
| `game_renderer.cpp` | Update key hints string | ~1 |

**Total: ~9 lines changed**

---

## 6. Edge Cases (All Already Handled)

| Case | How It's Handled |
|------|-----------------|
| Empty inventory | `sell_item()` returns 0, no gold added, cursor stays 0 |
| Cursor out of bounds | `sell_item()` returns 0 for invalid index |
| Cursor re-clamping | After sell, cursor is re-clamped to `min(cursor, max(0, item_count-1))` |
| Sell last item | Cursor goes to 0, inventory stays open |
| Room message display | Uses existing `room_msg` / `room_msg_timer` system |

---

## 7. Test Plan

### File: `tests/economy/inventory_sell_ui_test.cpp`

```cpp
TEST(InventorySellTest, SellItemReturnsGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<EquipmentItem>("Test Sword", Rarity::COMMON, "weapon", 8);
    p.inventory.add(item, &p);
    int val = p.inventory.sell_item(0, &p);
    EXPECT_EQ(val, 10);  // COMMON equipment = 10 gold
    EXPECT_EQ(p.get_gold(), 10);
    EXPECT_TRUE(p.inventory.items.empty());
}

TEST(InventorySellTest, SellInvalidIndexReturnsZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.inventory.sell_item(0, &p), 0);
    EXPECT_EQ(p.inventory.sell_item(-1, &p), 0);
}

TEST(InventorySellTest, SellConsumable) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<ConsumableItem>("Test Potion", Rarity::COMMON, "heal", 20);
    p.inventory.add(item, &p);
    int val = p.inventory.sell_item(0, &p);
    EXPECT_EQ(val, 5);  // COMMON consumable = 5 gold
    EXPECT_EQ(p.get_gold(), 5);
}
```

---

## 8. What's NOT in This Batch

- ❌ 商店
- ❌ 装备买回
- ❌ 批量出售
- ❌ 按品质自动出售
- ❌ 出售确认窗口
- ❌ 复杂动画
- ❌ 经济平衡重构

---

## 9. Review Gate Questions

1. **[T] for sell** — Confirmed. No conflicts.
2. **Room message for sell feedback** — Using existing `room_msg` system. Acceptable?
3. **No sell confirmation** — Single [T] press immediately sells. Acceptable for MVP?

---

*Design complete. Awaiting Review Gate before coding.*
