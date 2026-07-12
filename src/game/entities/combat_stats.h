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

// ============================================================
// B11: RelicInstance — 局内圣物 (挂在 Player 上, 跨楼层持续)
// ============================================================
struct RelicInstance {
    std::string id;   // "blood_charm" | "venom_fang" | ...
};

struct CombatStats {
    int max_hp;
    int current_hp;
    int attack;
    int physical_defense;
    int magical_defense;
    bool is_alive = true;
    std::unordered_map<std::string, float> modifiers;

    // D3 Step3 E3: 护盾 (SelfHeal Evo3 设置)
    float shield_hp = 0.0f;

    CombatStats(int hp = 20, int atk = 5, int pdef = 0, int mdef = 0);

    int take_damage(int amount);
    int heal(int amount);

    int get_effective_attack() const;
    int get_effective_defense(AttackType type = AttackType::PHYSICAL) const;
};
