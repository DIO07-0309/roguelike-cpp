#include "special_room.h"
#include "player.h"
#include "combat_system.h"
#include "item.h"
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
    int amount = std::max(1, player->combat.max_hp * 3 / 10);
    player->combat.heal(amount);
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
// B9.2 宝箱: 品质分层 (60% 普通 / 30% 丰厚 / 10% 祝福)
// ============================================================
static std::string _exec_treasure(Player* player) {
    int roll = rng() % 100; // 0-99

    if (roll < 10) {
        // 祝福宝箱 (10%)
        auto item = generate_random_item();
        if (item) player->inventory.add(item, player);
        apply_buff(player, "attack_up", 1);
        return "宝箱中的力量流入了你的身体。";
    } else if (roll < 40) {
        // 丰厚宝箱 (30%: roll ∈ [10, 39])
        auto a = generate_random_item();
        auto b = generate_random_item();
        if (a) player->inventory.add(a, player);
        if (b) player->inventory.add(b, player);
        return "你打开了宝箱，里面装着两件战利品！";
    } else {
        // 普通宝箱 (60%: roll ∈ [40, 99])
        auto item = generate_random_item();
        if (!item) return "宝箱里空空如也。";
        player->inventory.add(item, player);
        return "你打开了宝箱，获得了战利品。";
    }
}

// ============================================================
// B9.3 泉水: 回满 HP + 净化负面状态
// ============================================================
static std::string _exec_fountain(Player* player) {
    int missing = player->combat.max_hp - player->combat.current_hp;
    if (missing > 0) player->combat.heal(missing);
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
