#include "item.h"
#include "player.h"
#include "skill.h"

std::mt19937 rng((unsigned)time(nullptr));

Rarity random_rarity() {
    int w[] = {60, 25, 12, 3};
    int total = 100;
    int roll = (int)(rng() % total);
    int sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += w[i];
        if (roll < sum) return (Rarity)i;
    }
    return Rarity::COMMON;
}

void EquipmentItem::apply(Player* player) {
    auto& mod = player->combat.modifiers;
    mod["atk_flat"] = (mod.count("atk_flat") ? mod["atk_flat"] : 0) + atk_bonus;
    if (pdef_bonus > 0) mod["pdef_flat"] = (mod.count("pdef_flat") ? mod["pdef_flat"] : 0) + pdef_bonus;
    if (mdef_bonus > 0) mod["mdef_flat"] = (mod.count("mdef_flat") ? mod["mdef_flat"] : 0) + mdef_bonus;
}

void EquipmentItem::remove(Player* player) {
    auto& mod = player->combat.modifiers;
    mod["atk_flat"] = mod["atk_flat"] - atk_bonus;
    if (pdef_bonus > 0) mod["pdef_flat"] = mod["pdef_flat"] - pdef_bonus;
    if (mdef_bonus > 0) mod["mdef_flat"] = mod["mdef_flat"] - mdef_bonus;
}

std::string EquipmentItem::get_description() const {
    std::string d = rarity_label(rarity) + " " + base_name + " (";
    if (atk_bonus) d += "ATK+" + std::to_string(atk_bonus) + " ";
    if (pdef_bonus) d += "PD+" + std::to_string(pdef_bonus) + " ";
    if (mdef_bonus) d += "MD+" + std::to_string(mdef_bonus) + " ";
    d += ")";
    return d;
}

void CharmItem::apply(Player* player) {
    for (auto& s : player->skills.active_skills) {
        if (typeid(*s).name() == skill_class_name || 1) {
            s->apply_charm(cd_bonus, power_bonus);
            break;
        }
    }
}

void CharmItem::remove(Player* player) {
    for (auto& s : player->skills.active_skills) {
        s->remove_charm();
        break;
    }
}

std::string CharmItem::get_description() const {
    std::string d = rarity_label(rarity) + " " + base_name + " (";
    if (cd_bonus > 0) d += "冷却-" + std::to_string((int)(cd_bonus*100)) + "% ";
    if (power_bonus > 0) d += "威力+" + std::to_string((int)(power_bonus*100)) + "%";
    d += ")";
    return d;
}

std::string ConsumableItem::use(Player* player) {
    if (effect_type == "heal") {
        int recovered = player->combat.heal(effect_value);
        return "使用 " + get_description() + "，恢复了 " + std::to_string(recovered) + " HP";
    }
    return "使用了 " + get_description();
}

// ---- 随机生成 ----
std::shared_ptr<Item> generate_random_item() {
    Rarity r = random_rarity();
    int cat = (int)(rng() % 4);  // 0=weapon, 1=armor, 2=potion, 3=charm

    if (cat == 3) return generate_random_charm();

    const char* weapons[] = {"短剑", "长剑", "战斧", "匕首", "弯刀", "重锤", "长矛"};
    const char* armors[] = {"皮甲", "锁子甲", "铁铠", "鳞甲", "板甲"};
    const char* potions[] = {"生命药水", "治疗药剂", "恢复灵药"};

    if (cat == 0) {
        const char* n = weapons[rng() % 7];
        int atk = 5 + (int)(rng() % 8);
        return std::make_shared<EquipmentItem>(n, r, "weapon", atk, (int)(rng() % 3));
    } else if (cat == 1) {
        const char* n = armors[rng() % 5];
        int pd = 3 + (int)(rng() % 6);
        return std::make_shared<EquipmentItem>(n, r, "armor", 0, pd, (int)(rng() % 4));
    } else {
        const char* n = potions[rng() % 3];
        int heal = 15 + (int)(rng() % 26);
        return std::make_shared<ConsumableItem>(n, r, "heal", heal);
    }
}

std::shared_ptr<CharmItem> generate_charm_for_skill(const std::string& skill, Rarity r) {
    float m = rarity_mult(r);
    if (skill == "SlashSkill")
        return std::make_shared<CharmItem>("斩击纹章", r, "SlashSkill",
                                           0.15f * m, 0.40f * m);
    if (skill == "FireballSkill")
        return std::make_shared<CharmItem>("烈焰之心", r, "FireballSkill",
                                           0.20f * m, 0.50f * m);
    if (skill == "SelfHealSkill")
        return std::make_shared<CharmItem>("生命之泉", r, "SelfHealSkill",
                                           0.25f * m, 0.40f * m);
    if (skill == "TheWorldSkill")
        return std::make_shared<CharmItem>("时光沙漏", r, "TheWorldSkill",
                                           0.25f * m, 0.25f * m);
    return nullptr;
}

std::shared_ptr<CharmItem> generate_random_charm() {
    const char* skills[] = {"SlashSkill", "FireballSkill", "SelfHealSkill", "TheWorldSkill"};
    return generate_charm_for_skill(skills[rng() % 4]);
}
