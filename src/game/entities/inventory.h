#pragma once
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "item.h"

class Player;

// ============================================================
// Inventory — 背包系统
// ============================================================
class Inventory {
public:
    std::vector<std::shared_ptr<Item>> items;
    std::unordered_map<std::string, std::shared_ptr<EquipmentItem>> equipped;
    int max_size = 16;

    Inventory(int capacity = 16);

    bool is_full() const { return (int)items.size() >= max_size; }
    int item_count() const { return (int)items.size(); }

    bool add(std::shared_ptr<Item> item, Player* player);
    std::shared_ptr<Item> remove(int index);
    std::string equip(int index, Player* player);
    std::string unequip(const std::string& slot, Player* player);
    std::string use_item(int index, Player* player);
};
