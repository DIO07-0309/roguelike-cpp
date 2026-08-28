#include <gtest/gtest.h>
#include "entities/player.h"
#include "entities/inventory.h"
#include "entities/item.h"
#include "player_controller.h"
#include "director/presentation_system_director.h"

// Stub font globals (defined in main.cpp, excluded from test lib)
#include "raylib.h"
Font g_font = {0};
Font g_font_small = {0};
bool g_font_loaded = false;

TEST(InventorySellTest, SellEquipmentReturnsGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<EquipmentItem>("Sword", Rarity::COMMON, "weapon", 8);
    p.inventory.add(item, &p);
    int val = p.inventory.sell_item(0, &p);
    EXPECT_EQ(val, 10);
    EXPECT_EQ(p.get_gold(), 10);
    EXPECT_TRUE(p.inventory.items.empty());
}

TEST(InventorySellTest, SellConsumableReturnsGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<ConsumableItem>("Potion", Rarity::RARE, "heal", 20);
    p.inventory.add(item, &p);
    int val = p.inventory.sell_item(0, &p);
    EXPECT_EQ(val, 9);
    EXPECT_EQ(p.get_gold(), 9);
}

TEST(InventorySellTest, SellInvalidIndexReturnsZero) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    EXPECT_EQ(p.inventory.sell_item(0, &p), 0);
    EXPECT_EQ(p.inventory.sell_item(-1, &p), 0);
    EXPECT_EQ(p.inventory.sell_item(999, &p), 0);
}

TEST(InventorySellTest, SellEmptyInventoryNoop) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    int cursor = 0;
    PresentationSystemDirector pres;
    int val = PlayerController::sell_selected_item(p.inventory, &p, cursor, pres);
    EXPECT_EQ(val, 0);
    EXPECT_EQ(p.get_gold(), 0);
    EXPECT_EQ(cursor, 0);
}

TEST(InventorySellTest, SellLastItemClampsCursor) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<EquipmentItem>("Sword", Rarity::COMMON, "weapon", 8);
    p.inventory.add(item, &p);
    int cursor = 0;
    PresentationSystemDirector pres;
    int val = PlayerController::sell_selected_item(p.inventory, &p, cursor, pres);
    EXPECT_EQ(val, 10);
    EXPECT_EQ(p.get_gold(), 10);
    EXPECT_TRUE(p.inventory.items.empty());
    EXPECT_EQ(cursor, 0);
}

TEST(InventorySellTest, SellMiddleItemClampsCursor) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    for (int i = 0; i < 5; i++)
        p.inventory.add(std::make_shared<EquipmentItem>("Item" + std::to_string(i), Rarity::COMMON, "weapon", 5), &p);
    int cursor = 2;
    PresentationSystemDirector pres;
    PlayerController::sell_selected_item(p.inventory, &p, cursor, pres);
    EXPECT_EQ((int)p.inventory.items.size(), 4);
    EXPECT_EQ(cursor, 2);
}

TEST(InventorySellTest, SellSetsRoomMessage) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    auto item = std::make_shared<EquipmentItem>("Sword", Rarity::COMMON, "weapon", 8);
    p.inventory.add(item, &p);
    int cursor = 0;
    PresentationSystemDirector pres;
    PlayerController::sell_selected_item(p.inventory, &p, cursor, pres);
    EXPECT_NE(pres.room_msg.find("Gold"), std::string::npos);
    EXPECT_GT(pres.room_msg_timer, 0.0f);
}

TEST(InventorySellTest, SellMultipleItemsAccumulatesGold) {
    Player p(0, 0, 200, 100, 10, 5, 3);
    for (int i = 0; i < 3; i++)
        p.inventory.add(std::make_shared<EquipmentItem>("Item", Rarity::COMMON, "weapon", 5), &p);
    int cursor = 0;
    PresentationSystemDirector pres;
    for (int i = 0; i < 3; i++)
        PlayerController::sell_selected_item(p.inventory, &p, cursor, pres);
    EXPECT_EQ(p.get_gold(), 30);
    EXPECT_TRUE(p.inventory.items.empty());
}
