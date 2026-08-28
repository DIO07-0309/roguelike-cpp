#pragma once
#include <memory>
#include <string>
#include "../entities/combat_stats.h"

class Player;
struct Item;

// ============================================================
// Batch 3A: RewardManager — 最小奖励发放层
// 职责: 安全地将奖励写入 Player / Inventory / Relic
// 不负责: 概率, 随机生成, 地面掉落, UI, 存档
// ============================================================
class RewardManager {
public:
    static bool grant_item(Player& player, std::shared_ptr<Item> item);
    static void grant_gold(Player& player, int amount);
    static void grant_key(Player& player, int count);
    static bool grant_relic(Player& player, const std::string& relic_id,
                            PersistenceScope scope);
};
