#include "inventory.h"
#include "player.h"
#include "item.h"

Inventory::Inventory(int capacity) : max_size(capacity) {
    equipped["weapon"] = nullptr;
    equipped["armor"] = nullptr;
    equipped["charm"] = nullptr;
}

bool Inventory::add(std::shared_ptr<Item> item, Player* player) {
    if (is_full()) return false;
    items.push_back(item);
    return true;
}

std::shared_ptr<Item> Inventory::remove(int index) {
    if (index < 0 || index >= (int)items.size()) return nullptr;
    auto item = items[index];
    items.erase(items.begin() + index);
    return item;
}

std::string Inventory::equip(int index, Player* player) {
    if (index < 0 || index >= (int)items.size()) return "";
    auto item = std::dynamic_pointer_cast<EquipmentItem>(items[index]);
    if (!item) return items[index]->get_description() + " 不可装备";

    // 卸旧
    auto& slot = equipped[item->slot];
    if (slot) {
        slot->remove(player);
        items.push_back(slot);
    }
    // 穿新
    slot = item;
    items.erase(items.begin() + index);
    item->apply(player);
    // G9: sync WeaponComponent on weapon equip
    if (item->slot == "weapon" && !item->weapon_def_id.empty())
        player->weapon.equip(item->weapon_def_id);
    return "装备了 " + item->get_description();
}

std::string Inventory::unequip(const std::string& slot, Player* player) {
    if (!equipped.count(slot) || !equipped[slot]) return "";
    if (is_full()) return "背包已满";
    auto item = equipped[slot];
    item->remove(player);
    items.push_back(item);
    equipped[slot] = nullptr;
    // G9: reset to fist on weapon unequip
    if (slot == "weapon")
        player->weapon.unequip();
    return "卸下了 " + item->get_description();
}

std::string Inventory::use_item(int index, Player* player) {
    if (index < 0 || index >= (int)items.size()) return "";
    auto cons = std::dynamic_pointer_cast<ConsumableItem>(items[index]);
    if (!cons) return items[index]->get_description() + " 无法使用";
    std::string result = cons->use(player);
    items.erase(items.begin() + index);
    return result;
}

// Batch 3A: 出售物品
int Inventory::sell_item(int index, Player* player) {
    if (index < 0 || index >= (int)items.size()) return 0;
    int value = get_sell_value(items[index].get());
    items.erase(items.begin() + index);
    if (player) player->add_gold(value);
    return value;
}
