#include "build_score.h"
#include "player.h"
#include "combat_system.h"

// ============================================================
// BuildScore: 从玩家所有系统收集标签并累计
// ============================================================
BuildScore calculate_build(const Player* player) {
    BuildScore bs;
    if (!player) return bs;

    // 1. 主动技能 tags (每个技能 +1 权重, 进化等级额外加分)
    for (auto& sk : player->skills.active_skills) {
        for (auto t : sk->tags) bs.add(t, 1 + sk->evolution_level);
    }
    // 2. 被动技能 tags
    for (auto& pk : player->skills.passives) {
        for (auto t : pk->tags) bs.add(t, 1);
    }
    // 3. Relic favorite_tags (持有中的圣物)
    for (auto& r : player->relics) {
        const RelicDef* def = get_relic_def(r.id);
        if (!def) continue;
        for (auto t : def->favorite_tags) bs.add(t, 2);
    }
    // 4. 当前 Buff tags
    for (auto& b : player->active_buffs) {
        const BuffDef* def = get_buff_def(b.id);
        if (!def) continue;
        for (auto t : def->tags) bs.add(t, b.stacks);
    }
    return bs;
}

// ============================================================
// BuildType 判定 — 最高分的流派
// ============================================================
BuildType BuildScore::identify() const {
    struct Candidate { BuildType type; int min; BuildTag main; int sec_req; BuildTag sec; };
    Candidate cands[] = {
        {BuildType::BERSERKER,     5, BuildTag::MELEE,     3, BuildTag::COMBO},
        {BuildType::FIRE_MAGE,     4, BuildTag::FIRE,      2, BuildTag::DOT},
        {BuildType::POISON_MASTER, 4, BuildTag::POISON,    2, BuildTag::DOT},
        {BuildType::TIME_MASTER,   4, BuildTag::TIME,      2, BuildTag::SUPPORT},
        {BuildType::SUPPORT,        4, BuildTag::HEAL,     2, BuildTag::SUPPORT},
        {BuildType::PROJECTILE,    3, BuildTag::PROJECTILE,2, BuildTag::RANGED},
    };

    BuildType best = BuildType::NONE;
    float best_score = 0;
    for (auto& c : cands) {
        float s = (float)(get(c.main) + get(c.sec));
        if (get(c.main) >= c.min && get(c.sec) >= c.sec_req && s > best_score) {
            best_score = s; best = c.type;
        }
    }
    return best;
}

const char* BuildScore::build_name_cn(BuildType bt) const {
    switch (bt) {
        case BuildType::BERSERKER:     return "狂战士";
        case BuildType::FIRE_MAGE:     return "火法师";
        case BuildType::POISON_MASTER: return "毒术大师";
        case BuildType::TIME_MASTER:   return "时间术士";
        case BuildType::SUPPORT:       return "辅助者";
        case BuildType::PROJECTILE:    return "弹幕射手";
        default:                       return "无构筑";
    }
}
const char* BuildScore::build_name() const { return build_name_cn(identify()); }

float BuildScore::progress(BuildType bt) const {
    if (bt == BuildType::BERSERKER)     return std::min(1.0f, (float)get(BuildTag::MELEE) / 10.0f);
    if (bt == BuildType::FIRE_MAGE)     return std::min(1.0f, (float)get(BuildTag::FIRE) / 8.0f);
    if (bt == BuildType::POISON_MASTER) return std::min(1.0f, (float)get(BuildTag::POISON) / 8.0f);
    if (bt == BuildType::TIME_MASTER)   return std::min(1.0f, (float)get(BuildTag::TIME) / 6.0f);
    if (bt == BuildType::SUPPORT)       return std::min(1.0f, (float)get(BuildTag::HEAL) / 6.0f);
    if (bt == BuildType::PROJECTILE)    return std::min(1.0f, (float)get(BuildTag::PROJECTILE) / 5.0f);
    return 0.0f;
}

// ============================================================
// Relic 推荐权重 — 匹配 Build 的 relic 权重+60%
// ============================================================
bool relic_matches_build(const BuildScore& bs, const std::string& relic_id) {
    const RelicDef* def = get_relic_def(relic_id);
    if (!def) return false;
    for (auto t : def->favorite_tags)
        if (bs.get(t) >= 3) return true;  // 该标签得分≥3 → 匹配
    return false;
}

void recommend_relic_weights(const BuildScore& bs,
                              std::vector<std::string>& out_ids,
                              std::vector<int>& out_weights) {
    for (size_t i = 0; i < out_ids.size(); i++) {
        if (relic_matches_build(bs, out_ids[i]))
            out_weights[i] = out_weights[i] * 16 / 10;  // +60% weight
    }
}

// ============================================================
// Build Bonus — 玩法变化
// ============================================================
BuildBonus get_build_bonus(BuildType bt) {
    BuildBonus bb;
    switch (bt) {
    case BuildType::POISON_MASTER:
        bb.poison_duration_boost = 2.0f;
        bb.poison_tick_speed = 0.20f;
        break;
    case BuildType::BERSERKER:
        bb.heavy_heal = true;
        bb.heavy_keep_combo = true;
        break;
    case BuildType::TIME_MASTER:
        bb.time_duration_boost = 0.15f;
        break;
    case BuildType::FIRE_MAGE:
        bb.fire_extra_dot = 1.0f;
        break;
    default: break;
    }
    return bb;
}
