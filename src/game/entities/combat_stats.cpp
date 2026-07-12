#include "combat_stats.h"
#include <algorithm>

CombatStats::CombatStats(int hp, int atk, int pdef, int mdef)
    : max_hp(hp), current_hp(hp), attack(atk),
      physical_defense(pdef), magical_defense(mdef) {}

int CombatStats::take_damage(int amount) {
    if (!is_alive) return 0;
    if (amount < 0) amount = 0;
    // D3 Step3: 护盾吸收 (shield_hp先扣)
    if (shield_hp > 0) {
        int absorbed = (int)std::min((float)amount, shield_hp);
        shield_hp -= (float)absorbed;
        amount -= absorbed;
        if (amount <= 0) return absorbed; // 护盾完全吸收
    }
    current_hp -= amount;
    if (current_hp <= 0) { current_hp = 0; is_alive = false; }
    return amount;
}

int CombatStats::heal(int amount) {
    if (!is_alive || amount <= 0) return 0;
    int old = current_hp;
    current_hp = std::min(max_hp, current_hp + amount);
    return current_hp - old;
}

int CombatStats::get_effective_attack() const {
    int bonus = static_cast<int>(modifiers.count("atk_flat") ? modifiers.at("atk_flat") : 0);
    float mult = modifiers.count("atk_pct") ? modifiers.at("atk_pct") : 0.0f;
    int val = static_cast<int>(attack * (1.0f + mult)) + bonus;
    return std::max(1, val);
}

int CombatStats::get_effective_defense(AttackType type) const {
    if (type == AttackType::TRUE) return 0;
    bool is_phys = (type == AttackType::PHYSICAL);
    int base = is_phys ? physical_defense : magical_defense;
    const char* slot_key = is_phys ? "pdef_flat" : "mdef_flat";
    int def_flat = static_cast<int>(modifiers.count("def_flat") ? modifiers.at("def_flat") : 0);
    int slot = static_cast<int>(modifiers.count(slot_key) ? modifiers.at(slot_key) : 0);
    float mult = modifiers.count("def_pct") ? modifiers.at("def_pct") : 0.0f;
    int val = static_cast<int>(base * (1.0f + mult)) + def_flat + slot;
    return std::max(0, val);
}
