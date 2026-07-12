#include "special_room.h"
#include "player.h"
#include "combat_system.h"
#include "item.h"
#include "core/logger.h"
#include "build_score.h"
#include "relic_progression.h"
#include <algorithm>

SpecialRoomType special_room_from_index(int idx) {
    switch (idx % 3) {
        case 0: return SpecialRoomType::ALTAR;
        case 1: return SpecialRoomType::TREASURE;
        default: return SpecialRoomType::FOUNTAIN;
    }
}

std::string special_room_to_string(SpecialRoomType type) {
    switch (type) {
        case SpecialRoomType::ALTAR:    return "altar";
        case SpecialRoomType::TREASURE: return "treasure";
        case SpecialRoomType::FOUNTAIN: return "fountain";
    }
    return "unknown";
}

SpecialRoomType special_room_from_string(const std::string& s) {
    if (s == "altar")    return SpecialRoomType::ALTAR;
    if (s == "treasure") return SpecialRoomType::TREASURE;
    if (s == "fountain") return SpecialRoomType::FOUNTAIN;
    return SpecialRoomType::ALTAR; // safe fallback
}

// ============================================================
// B9 共享辅助: 净化负面 Buff (poison / slow)
// ============================================================
static void _cleanse_debuffs(Player* player) {
    auto& buffs = player->active_buffs;
    buffs.erase(std::remove_if(buffs.begin(), buffs.end(),
        [](const BuffInstance& b) {
            return b.id == "poison" || b.id == "slow";
        }), buffs.end());
}

// ============================================================
// B9.1 祭坛: 4 结果池 (等概率随机)
// ============================================================
static std::string _altar_attack_up(Player* player) {
    apply_buff(player, "attack_up", 1);
    return "祭坛赐福：你的攻击力提升了。";
}

static std::string _altar_heal(Player* player) {
    int eff_max = get_effective_max_hp(player);  // B11: blood_charm
    int amount = std::max(1, eff_max * 3 / 10);
    // B12: sage_leaf — 祭坛治疗 +10
    if (player_has_relic(player, "sage_leaf")) amount += 10;
    heal_player(player, amount);  // B11: 使用 effective max clamp
    return "祭坛赐福：你的伤势恢复了。";
}

static std::string _altar_blood_power(Player* player) {
    int loss = std::max(1, player->combat.current_hp * 2 / 10);
    if (player->combat.current_hp - loss < 1)
        loss = player->combat.current_hp - 1;
    if (loss > 0) player->combat.take_damage(loss);
    apply_buff(player, "attack_up", 2);
    return "祭坛夺取了你的生命，但赐予了更强的力量。";
}

static std::string _altar_cleanse(Player* player) {
    _cleanse_debuffs(player);
    return "祭坛的光辉净化了你身上的负面状态。";
}

static std::string _exec_altar(Player* player) {
    using Fn = std::string(*)(Player*);
    static const Fn pool[] = {
        _altar_attack_up,
        _altar_heal,
        _altar_blood_power,
        _altar_cleanse,
    };
    int idx = rng() % 4;
    return pool[idx](player);
}

// ============================================================
// B12: rarity 权重 relic 抽取 (common:100, rare:40, epic:10)
static int _rarity_weight(const std::string& rarity) {
    if (rarity == "common") return 100;
    if (rarity == "rare")   return 40;
    if (rarity == "epic")   return 10;
    return 100; // fallback
}

// B12: 宝箱房 relic 掉落 — 按 rarity 权重抽取, 支持 drop_chance 参数
static std::string _try_grant_random_relic(Player* player, float drop_chance) {
    if (!player) return "";

    if ((int)(rng() % 1000) >= (int)(drop_chance * 1000.0f)) return "";

    auto all_ids = get_all_relic_ids();
    // 先按 rarity 权重选 rarity 档
    struct RaritySlot { std::string rarity; int weight; std::vector<std::string> ids; };
    RaritySlot slots[] = {{"common", 100, {}}, {"rare", 40, {}}, {"epic", 10, {}}};
    int total_w = 0;
    for (auto& slot : slots) {
        for (auto& id : all_ids) {
            if (!player_has_relic(player, id)) {
                const RelicDef* def = get_relic_def(id);
                if (def && def->rarity == slot.rarity)
                    slot.ids.push_back(id);
            }
        }
        if (!slot.ids.empty()) total_w += slot.weight;
    }

    // D3 Step4: 根据 BuildScore 调整 relic 权重 (匹配+60%)
    {
        BuildScore bs = calculate_build(player);
        for (auto& slot : slots) {
            std::vector<std::string> matched;
            for (auto& id : slot.ids)
                if (relic_matches_build(bs, id))
                    matched.push_back(id);
            if (!matched.empty()) slot.weight = slot.weight * 8 / 5; // +60%
        }
    }

    if (total_w == 0) return "";

    int roll = (int)(rng() % (uint32_t)total_w);
    std::string chosen;
    for (auto& slot : slots) {
        if (slot.ids.empty()) continue;
        if (roll < slot.weight) {
            // D3 Step4: 匹配 Build 的 relic 概率翻倍
            std::vector<std::string>& pool = slot.ids;
            std::vector<std::string> build_ids;
            BuildScore bs2 = calculate_build(player);
            for (auto& id : pool)
                if (relic_matches_build(bs2, id)) build_ids.push_back(id);
            if (!build_ids.empty() && (rng() % 100) < 40)
                chosen = build_ids[(int)(rng() % (uint32_t)build_ids.size())];
            else
                chosen = pool[(int)(rng() % (uint32_t)pool.size())];
            break;
        }
        roll -= slot.weight;
    }
    // fallback: 如果因某种原因没选中, 从所有候选中随机
    if (chosen.empty()) {
        std::vector<std::string> all_candidates;
        for (auto& id : all_ids)
            if (!player_has_relic(player, id))
                all_candidates.push_back(id);
        if (all_candidates.empty()) return "";
        chosen = all_candidates[(int)(rng() % (uint32_t)all_candidates.size())];
    }

    player->relics.push_back({chosen});
    const RelicDef* def = get_relic_def(chosen);
    std::string name = def ? def->name : chosen;
    LOG_INFO("[RELIC] 获得圣物: %s (%s)", name.c_str(), def ? def->rarity.c_str() : "?");
    // D4.6 Step4: 记录到永久Archive + 熟练度
    if (def) g_relic_archive.mark_obtained(chosen, rarity_level(def->rarity));
    // C1: 圣物获得仪式感 — 特殊消息格式
    return "RELIC:" + name;  // GameScene 会识别 RELIC: 前缀显示大提示
}

// ============================================================
// B9.2 宝箱: 品质分层 (60% 普通 / 30% 丰厚 / 10% 祝福)
// ============================================================
static std::string _exec_treasure(Player* player) {
    int roll = rng() % 100; // 0-99
    std::string msg;

    if (roll < 10) {
        // 祝福宝箱 (10%)
        auto item = generate_random_item();
        if (item) player->inventory.add(item, player);
        apply_buff(player, "attack_up", 1);
        msg = "宝箱中的力量流入了你的身体。";
    } else if (roll < 40) {
        // 丰厚宝箱 (30%: roll ∈ [10, 39])
        auto a = generate_random_item();
        auto b = generate_random_item();
        if (a) player->inventory.add(a, player);
        if (b) player->inventory.add(b, player);
        msg = "你打开了宝箱，里面装着两件战利品！";
    } else {
        // 普通宝箱 (60%: roll ∈ [40, 99])
        auto item = generate_random_item();
        if (!item) {
            msg = "宝箱里空空如也。";
        } else {
            player->inventory.add(item, player);
            msg = "你打开了宝箱，获得了战利品。";
        }
    }

    // B13: 宝箱品质分级 relic 掉率 (普通40% / 丰厚70% / 祝福100%)
    float relic_chance = (roll < 10) ? 1.00f : (roll < 40) ? 0.70f : 0.40f;
    // B12: merchant_coin — relic 掉率 +15%
    if (player_has_relic(player, "merchant_coin")) {
        relic_chance += 0.15f;
        if (relic_chance > 0.95f) relic_chance = 0.95f;
    }
    {
        std::string relic_msg = _try_grant_random_relic(player, relic_chance);
        if (!relic_msg.empty()) msg += " " + relic_msg;
    }
    // B11: golden_dice — 宝箱额外多给 1 件物品
    if (player_has_relic(player, "golden_dice")) {
        auto extra = generate_random_item();
        if (extra && player->inventory.add(extra, player))
            msg += " 金色骰子额外赐予了一件物品。";
    }
    // B12: merchant_coin — 宝箱额外多给 1 件物品 (与 golden_dice 叠加)
    if (player_has_relic(player, "merchant_coin")) {
        auto extra = generate_random_item();
        if (extra && player->inventory.add(extra, player))
            msg += " 商人硬币额外带来了一件物品。";
    }
    return msg;
}

// ============================================================
// B9.3 泉水: 回满 HP + 净化负面状态
// ============================================================
static std::string _exec_fountain(Player* player) {
    int eff_max = get_effective_max_hp(player);  // B11: blood_charm
    int missing = eff_max - player->combat.current_hp;
    if (missing < 0) missing = 0;
    // B12: sage_leaf — 泉水治疗 +10
    if (player_has_relic(player, "sage_leaf")) missing += 10;
    if (missing > 0) heal_player(player, missing);  // B11: 使用 effective max clamp
    _cleanse_debuffs(player);
    return "泉水治愈并净化了你的身体。";
}

// ============================================================
// 统一交互入口
// ============================================================
std::string execute_special_room(SpecialRoomType type, Player* player) {
    switch (type) {
        case SpecialRoomType::ALTAR:    return _exec_altar(player);
        case SpecialRoomType::TREASURE: return _exec_treasure(player);
        case SpecialRoomType::FOUNTAIN: return _exec_fountain(player);
    }
    return "未知房间。";
}
