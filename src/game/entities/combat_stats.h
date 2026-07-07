#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

// ============================================================
// CombatStats — 战斗属性组件 (组合模式)
// ============================================================
enum class AttackType { PHYSICAL, MAGICAL, TRUE };

// ============================================================
// BuffInstance — 单个 buff 实例 (挂在 Player / Monster 上)
// ============================================================
struct BuffInstance {
    std::string id;          // "poison" | "slow" | "attack_up"
    int stacks = 1;          // 当前层数 (1 ~ max_stacks)
    float remaining = 0.0f;  // 剩余时间 (秒)
    float tick_timer = 0.0f; // 距下一次 DOT 触发 (仅 poison 等周期 buff 使用)
};

struct CombatStats {
    int max_hp;
    int current_hp;
    int attack;
    int physical_defense;
    int magical_defense;
    bool is_alive = true;
    std::unordered_map<std::string, float> modifiers;

    CombatStats(int hp = 20, int atk = 5, int pdef = 0, int mdef = 0)
        : max_hp(hp), current_hp(hp), attack(atk),
          physical_defense(pdef), magical_defense(mdef) {}

    int take_damage(int amount) {
        if (!is_alive) return 0;
        if (amount < 0) amount = 0;
        current_hp -= amount;
        if (current_hp <= 0) { current_hp = 0; is_alive = false; }
        return amount;
    }

    int heal(int amount) {
        if (!is_alive || amount <= 0) return 0;
        int old = current_hp;
        current_hp = std::min(max_hp, current_hp + amount);
        return current_hp - old;
    }

    int get_effective_attack() const {
        int bonus = static_cast<int>(modifiers.count("atk_flat") ? modifiers.at("atk_flat") : 0);
        float mult = modifiers.count("atk_pct") ? modifiers.at("atk_pct") : 0.0f;
        int val = static_cast<int>(attack * (1.0f + mult)) + bonus;
        return std::max(1, val);
    }

    int get_effective_defense(AttackType type = AttackType::PHYSICAL) const {
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
};
