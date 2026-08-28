#include "reward_manager.h"
#include "../entities/player.h"
#include "../entities/combat_stats.h"
#include "../systems/combat_system.h"
#include "../relic_progression.h"

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
