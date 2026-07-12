#include "relic_progression.h"
#include "combat_system.h"
#include <algorithm>

// ============================================================
// D4.6 Step4: Relic Progression — 圣物 Archive
// ============================================================
RelicArchiveSystem g_relic_archive;

int rarity_level(const std::string& r) {
    if (r == "rare")   return 1;
    if (r == "epic")   return 2;
    if (r == "legendary" || r == "mythic") return 3;
    return 0; // common
}

// 套装加成: {tag, 需N件, bonus_desc, value}
// e.g. 收集3件MELEE relic → 永久ATK+2%
RelicArchiveSystem::RelicArchiveSystem() {
    _bonuses = {
        {"MELEE",      3, "近战伤害+2%",     0.02f},
        {"POISON",     3, "中毒持续+10%",    0.10f},
        {"FIRE",       3, "火焰伤害+5%",     0.05f},
        {"TIME",       2, "冷却缩减-3%",     -0.03f},
        {"DEFENSE",    3, "最大HP+5%",       0.05f},
        {"HEAL",       2, "治疗量+8%",       0.08f},
        {"SUPPORT",    3, "Buff持续+1s",     1.00f},
        {"PROJECTILE", 2, "弹幕速度+10%",    0.10f},
    };
}

// 标记获得 (每局每次获得调用一次)
void RelicArchiveSystem::mark_obtained(const std::string& relic_id, int rarity) {
    // 查找或新建entry
    RelicArchiveEntry* e = nullptr;
    for (auto& entry : _entries) {
        if (entry.id == relic_id) { e = &entry; break; }
    }
    if (!e) {
        RelicArchiveEntry neu;
        neu.id = relic_id;
        neu.permanent_unlock = true;  // 首次获得永久解锁
        _entries.push_back(neu);
        e = &_entries.back();
    }
    e->obtained_count++;
    if (rarity > e->highest_rarity) e->highest_rarity = rarity;
    e->mastery = std::min(500, e->mastery + 50);
    e->permanent_unlock = true;
}

bool RelicArchiveSystem::is_unlocked(const std::string& relic_id) const {
    for (auto& e : _entries)
        if (e.id == relic_id && e.permanent_unlock) return true;
    return false;
}

int RelicArchiveSystem::mastery_level(const std::string& relic_id) const {
    // 0=未解锁, 1=100, 2=200, 3=300, 4=400, 5=500
    for (auto& e : _entries)
        if (e.id == relic_id)
            return std::min(5, e.mastery / 100);
    return 0;
}

int RelicArchiveSystem::collected_count() const {
    return (int)_entries.size();
}

int RelicArchiveSystem::total_relic_count() const {
    return (int)get_all_relic_ids().size();
}

float RelicArchiveSystem::collection_pct() const {
    int total = total_relic_count();
    if (total == 0) return 0;
    return (float)collected_count() / total;
}

float RelicArchiveSystem::get_collection_bonus(const std::string& tag) const {
    // 统计该tag下永久解锁的relic数量
    int count = 0;
    for (auto& e : _entries) {
        if (!e.permanent_unlock) continue;
        const RelicDef* def = get_relic_def(e.id);
        if (!def) continue;
        for (auto t : def->favorite_tags)
            if (std::string(build_tag_name(t)) == tag) { count++; break; }
    }
    // 查找对应bonus
    float bonus = 0;
    for (auto& b : _bonuses)
        if (b.tag == tag && count >= b.required_count)
            bonus = std::max(bonus, b.value);
    return bonus;
}

const RelicArchiveEntry* RelicArchiveSystem::entry(const std::string& relic_id) const {
    for (auto& e : _entries)
        if (e.id == relic_id) return &e;
    return nullptr;
}
