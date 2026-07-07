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

inline float rarity_mult(Rarity r) {
    float m[] = {1.0f, 1.5f, 2.0f, 3.0f};
    return m[static_cast<int>(r)];
}

inline Color rarity_color(Rarity r) {
    Color c[] = {
        {180, 180, 180, 255},  // COMMON
        {80, 160, 255, 255},   // RARE
        {180, 80, 255, 255},   // EPIC
        {255, 140, 40, 255},   // LEGENDARY
    };
    return c[static_cast<int>(r)];
}

inline std::string rarity_label(Rarity r) {
    const char* l[] = {"普通", "稀有", "史诗", "传说"};
    return l[static_cast<int>(r)];
}

// 基类
struct Item {
    std::string base_name;
    Rarity rarity;
    std::string tile_char;
    Color color;

    Item(const std::string& name, Rarity r, const std::string& tc)
        : base_name(name), rarity(r), tile_char(tc), color(rarity_color(r)) {}

    virtual ~Item() = default;
    virtual std::string get_description() const {
        return rarity_label(rarity) + " " + base_name;
    }
};

// 装备
class Player;
struct EquipmentItem : Item {
    std::string slot;  // "weapon", "armor", "charm"
    int atk_bonus = 0, pdef_bonus = 0, mdef_bonus = 0;

    EquipmentItem(const std::string& name, Rarity r, const std::string& sl,
                  int atk = 0, int pdef = 0, int mdef = 0)
        : Item(name, r, (sl == "weapon" ? "W" : sl == "armor" ? "A" : "C")),
          slot(sl) {
        float m = rarity_mult(r);
        atk_bonus = std::max(1, (int)(atk * m));
        pdef_bonus = std::max(1, (int)(pdef * m));
        mdef_bonus = std::max(1, (int)(mdef * m));
    }

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
              float cd, float pw)
        : EquipmentItem(name, r, "charm", 0, 0, 0),
          skill_class_name(sk), cd_bonus(cd), power_bonus(pw) {
        tile_char = "C";
    }
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
                   const std::string& buf = "")
        : Item(name, r, "P"), effect_type(type),
          effect_value(std::max(1, (int)(val * rarity_mult(r)))),
          buff_id(buf) {
        if (!buf.empty()) triggers = {{buf, 1, 1.0f, BuffTarget::SELF}};
    }

    std::string use(Player* player);
    std::string get_description() const override {
        if (!triggers.empty()) return rarity_label(rarity) + " " + base_name + " (" + get_buff_display_name(triggers[0].buff_id) + " 6s)";
        if (effect_type == "heal") return rarity_label(rarity) + " " + base_name + " (恢复" + std::to_string(effect_value) + " HP)";
        return Item::get_description();
    }
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
