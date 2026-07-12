#include "interaction_handler.h"
#include "player.h"
#include "game_map.h"
#include "special_room.h"
#include "combat_system.h"
#include "config.h"
#include <cmath>
#include <algorithm>

std::string InteractionHandler::try_interact(Player* player, GameMap* map,
                                              std::vector<DroppedItem>& ground_items) {
    if (!player || !map) return "";

    // 先检查特殊房间
    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    SpecialRoom* room = map->get_special_room_at(tx, ty);
    if (room && !room->triggered) {
        std::string msg = execute_special_room(room->type, player);
        room->triggered = true;
        return msg;
    }

    // 否则拾取物品
    return pickup_item(player, ground_items);
}

std::string InteractionHandler::pickup_item(Player* player,
                                             std::vector<DroppedItem>& ground_items) {
    DroppedItem* best = nullptr;
    float best_dist = PICKUP_RANGE * TILE_SIZE;
    Rectangle pr = player->entity.rect;

    for (auto& d : ground_items) {
        float px = d.tile_x * TILE_SIZE + TILE_SIZE / 2;
        float py = d.tile_y * TILE_SIZE + TILE_SIZE / 2;
        float dist = sqrtf(powf(pr.x + pr.width / 2 - px, 2)
                         + powf(pr.y + pr.height / 2 - py, 2));
        if (dist < best_dist) { best_dist = dist; best = &d; }
    }

    if (!best) return "";

    if (player->inventory.add(best->item, player)) {
        std::string desc = best->item->get_description();
        auto it = std::find_if(ground_items.begin(), ground_items.end(),
            [&](auto& d) { return &d == best; });
        if (it != ground_items.end()) ground_items.erase(it);
        return "拾取: " + desc;
    }
    return "";
}

std::string InteractionHandler::check_special_discovery(Player* player, GameMap* map) {
    if (!map || !player) return "";

    auto [tx, ty] = map->pixel_to_tile(
        player->entity.rect.x + player->entity.rect.width / 2,
        player->entity.rect.y + player->entity.rect.height / 2);

    SpecialRoom* room = map->get_special_room_at(tx, ty);
    if (!room || room->discovered) return "";

    room->discovered = true;
    switch (room->type) {
        case SpecialRoomType::ALTAR:  return "你发现了一座古老祭坛。";
        case SpecialRoomType::TREASURE: return "你发现了一个隐藏宝箱房。";
        case SpecialRoomType::FOUNTAIN: return "你发现了一处治愈泉水。";
    }
    return "";
}
