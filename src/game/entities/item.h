#pragma once
#include <vector>
#include <memory>
#include <random>
#include <cstdint>
#include <string>
#include "raylib.h"
#include "combat_system.h"

// ============================================================
// Item — 道具系统 (4稀有度 / 装备 / 消耗 / 护符)
// ============================================================

enum class Rarity {
    COMMON = 0,   // 1.0x, 灰
    RARE = 1,     // 1.5x, 蓝
    EPIC = 2,     // 2.0x, 紫
    LEGENDARY = 3, // 3.0x, 橙
};

float rarity_mult(Rarity r);
Color rarity_color(Rarity r);
std::string rarity_label(Rarity r);

// 基类
struct Item {
    std::string base_name;
    Rarity rarity;
    std::string tile_char;
    Color color;

    Item(const std::string& name, Rarity r, const std::string& tc);
    virtual ~Item() = default;
    virtual std::string get_description() const;
};

// 装备
class Player;
struct EquipmentItem : Item {
    std::string slot;  // "weapon", "armor", "charm"
    int atk_bonus = 0, pdef_bonus = 0, mdef_bonus = 0;

    EquipmentItem(const std::string& name, Rarity r, const std::string& sl,
                  int atk = 0, int pdef = 0, int mdef = 0);

    void apply(Player* player);
    void remove(Player* player);

    std::string get_description() const override;
};

// 护符
struct CharmItem : EquipmentItem {
    std::string skill_class_name;
    float cd_bonus = 0.0f;
    float power_bonus = 0.0f;

    CharmItem(const std::string& name, Rarity r, const std::string& sk,
              float cd, float pw);
    void apply(Player* player);
    void remove(Player* player);
    std::string get_description() const override;
};

// 消耗品
struct ConsumableItem : Item {
    std::string effect_type;
    int effect_value;
    std::string buff_id;                 // 兼容旧存档，新代码优先读 triggers
    std::vector<BuffTrigger> triggers;   // 使用后触发的 Buff 规则

    ConsumableItem(const std::string& name, Rarity r,
                   const std::string& type, int val,
                   const std::string& buf = "");

    std::string use(Player* player);
    std::string get_description() const override;
};

// 地面掉落物
struct DroppedItem {
    std::shared_ptr<Item> item;
    int tile_x = 0, tile_y = 0;
};

// 工厂
extern std::mt19937 rng;
Rarity random_rarity();
std::shared_ptr<Item> generate_random_item();
std::shared_ptr<CharmItem> generate_charm_for_skill(const std::string& skill_class, Rarity r = Rarity::COMMON);
std::shared_ptr<CharmItem> generate_random_charm();
