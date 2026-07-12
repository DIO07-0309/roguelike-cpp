#pragma once
#include <vector>
#include <string>
#include <cstdint>

// ============================================================
// D3 Step1: BuildTag — 构筑标签系统（所有 Build 的底层统一）
// 未来所有 Skill / Relic / Buff / MonsterSkill / Boss 全部基于此
// ============================================================

enum class BuildTag {
    NONE = 0,
    MELEE,      // 近战
    RANGED,     // 远程
    MAGIC,      // 法术
    FIRE,       // 火焰
    ICE,        // 冰霜
    LIGHTNING,  // 雷电
    POISON,     // 中毒
    BLEED,      // 流血
    SUMMON,     // 召唤
    COMBO,      // 连击
    HEAVY,      // 重击
    AOE,        // 范围伤害
    PROJECTILE, // 弹幕/投射物
    HEAL,       // 恢复
    TIME,       // 时间
    DEFENSE,    // 防御
    SUPPORT,    // 辅助
    DOT,        // 持续伤害 (Damage Over Time)
    KNOCKBACK,  // 击退
    COUNT
};

// ---- 通用接口: 标签查询 ----
// 作用于任意 vector<BuildTag>。Skill/Relic/Buff 均可直接调用。

inline bool has_tag(const std::vector<BuildTag>& tags, BuildTag tag) {
    for (auto t : tags) if (t == tag) return true;
    return false;
}

inline bool has_any_tag(const std::vector<BuildTag>& tags, std::initializer_list<BuildTag> list) {
    for (auto t : list) if (has_tag(tags, t)) return true;
    return false;
}

inline bool has_all_tags(const std::vector<BuildTag>& tags, std::initializer_list<BuildTag> list) {
    for (auto t : list) if (!has_tag(tags, t)) return false;
    return true;
}

// ---- 标签名称 (调试/HUD) ----
inline const char* build_tag_name(BuildTag t) {
    switch (t) {
        case BuildTag::MELEE: return "近战";
        case BuildTag::RANGED: return "远程";
        case BuildTag::MAGIC: return "法术";
        case BuildTag::FIRE: return "火焰";
        case BuildTag::ICE: return "冰霜";
        case BuildTag::LIGHTNING: return "雷电";
        case BuildTag::POISON: return "中毒";
        case BuildTag::BLEED: return "流血";
        case BuildTag::SUMMON: return "召唤";
        case BuildTag::COMBO: return "连击";
        case BuildTag::HEAVY: return "重击";
        case BuildTag::AOE: return "范围";
        case BuildTag::PROJECTILE: return "弹幕";
        case BuildTag::HEAL: return "恢复";
        case BuildTag::TIME: return "时间";
        case BuildTag::DEFENSE: return "防御";
        case BuildTag::SUPPORT: return "辅助";
        case BuildTag::DOT: return "持续";
        case BuildTag::KNOCKBACK: return "击退";
        default: return "?";
    }
}
