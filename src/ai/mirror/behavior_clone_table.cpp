// M1: BehaviorCloneTable implementation
#include "ai/mirror/behavior_clone_table.h"
#include <algorithm>
#include <cstdio>

// ── Intent mapping ──────────────────────────────────────
const char* intention_name(PlayerIntention i) {
    switch (i) {
    case PlayerIntention::ATTACK: return "ATTACK";
    case PlayerIntention::SKILL:  return "SKILL";
    case PlayerIntention::DODGE:  return "DODGE";
    case PlayerIntention::HEAL:   return "HEAL";
    case PlayerIntention::ADVANCE:return "ADVANCE";
    case PlayerIntention::RETREAT:return "RETREAT";
    default:                      return "IDLE";
    }
}

PlayerIntention intention_from_action(PlayerActionType type) {
    switch (type) {
    case PlayerActionType::ATTACK:      return PlayerIntention::ATTACK;
    case PlayerActionType::SKILL:       return PlayerIntention::SKILL;
    case PlayerActionType::DODGE:       return PlayerIntention::DODGE;
    case PlayerActionType::HEAL:        return PlayerIntention::HEAL;
    case PlayerActionType::DEAL_DAMAGE: return PlayerIntention::ATTACK;
    default:                            return PlayerIntention::IDLE;
    }
}

// ── Context bucketing ───────────────────────────────────
static int dist_bucket(float dist_tiles) {
    if (dist_tiles < 2.0f) return 0;      // 贴身
    if (dist_tiles < 4.0f) return 1;      // 近
    if (dist_tiles < 8.0f) return 2;      // 中
    if (dist_tiles < 14.0f) return 3;     // 远
    return 4;                             // 极远
}

static int hp_bucket(float hp_pct) {
    if (hp_pct <= 0.25f) return 0;        // 危急
    if (hp_pct < 0.50f) return 1;         // 低
    if (hp_pct < 0.80f) return 2;         // 中
    return 3;                             // 高
}

static int skill_bucket(int skills_ready) {
    return std::min(3, std::max(0, skills_ready));
}

CloneContext CloneContext::from_state(float dist_tiles, float hp_pct,
    int skills_ready) {
    CloneContext c;
    c.dist = dist_bucket(dist_tiles);
    c.hp = hp_bucket(hp_pct);
    c.skill = skill_bucket(skills_ready);
    return c;
}

std::string CloneContext::key() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "d%d:h%d:s%d", dist, hp, skill);
    return std::string(buf);
}

std::string CloneContext::fuzzy_key() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "d%d:h%d", dist, hp);
    return std::string(buf);
}

// ── Table operations ────────────────────────────────────
void BehaviorCloneTable::clear() {
    _table.clear();
    _has_profile = false;
    profile_ = PlayerHabitProfile{};
}

void BehaviorCloneTable::build(const std::vector<PlayerAction>& stream) {
    for (const auto& a : stream) {
        PlayerIntention intent = intention_from_action(a.type);
        if (intent == PlayerIntention::IDLE) continue;   // non-decision actions
        float dist = a.enemy_dist >= 0 ? a.enemy_dist : 2.0f;   // unknown -> mid
        float hp = a.hp >= 0 ? a.hp : 1.0f;                      // unknown -> full
        CloneContext ctx = CloneContext::from_state(dist, hp, a.skill_ready_mask);
        record_decision(ctx, intent);
    }
}

void BehaviorCloneTable::record_decision(const CloneContext& ctx,
    PlayerIntention intent) {
    if (intent < PlayerIntention::ATTACK || intent >= PlayerIntention::COUNT) return;
    _bump(_table[ctx.key()], intent);
}

bool BehaviorCloneTable::_pick_best(const Counts& c, ClonePrediction& out) {
    int total = 0, best_n = 0;
    for (int i = 0; i < (int)PlayerIntention::COUNT; i++) {
        total += c[i];
        if (c[i] > best_n) { best_n = c[i]; out.best = (PlayerIntention)i; }
    }
    if (total <= 0) return false;
    out.confidence = (float)best_n / (float)total;
    return true;
}

ClonePrediction BehaviorCloneTable::_predict_exact(const CloneContext& ctx) const {
    ClonePrediction p;
    p.level = -1;                             // -1 = miss
    auto it = _table.find(ctx.key());
    if (it != _table.end() && _pick_best(it->second, p)) p.level = 0;
    return p;
}

ClonePrediction BehaviorCloneTable::_predict_fuzzy(const CloneContext& ctx) const {
    // Merge the skill dimension: best intent across all skill buckets of (d, h)
    ClonePrediction p;
    p.level = -1;                             // -1 = miss
    Counts merged{};
    bool found = false;
    for (int s = 0; s <= 3; s++) {
        CloneContext c = ctx;
        c.skill = s;
        auto it = _table.find(c.key());
        if (it == _table.end()) continue;
        found = true;
        for (int i = 0; i < (int)PlayerIntention::COUNT; i++) merged[i] += it->second[i];
    }
    if (found && _pick_best(merged, p)) p.level = 1;
    return p;
}

PlayerIntention BehaviorCloneTable::fallback_intent(const PlayerHabitProfile& p,
    float dist_tiles, float hp_pct) {
    if (hp_pct < p.hp_counter_threshold / 100.0f && p.heal_frequency > 0.01f)
        return PlayerIntention::HEAL;
    if (p.predict_skill_spam && dist_tiles < 4.0f)
        return PlayerIntention::SKILL;
    if (p.predict_low_dodge && dist_tiles > 6.0f)
        return PlayerIntention::RETREAT;
    return PlayerIntention::ATTACK;
}

ClonePrediction BehaviorCloneTable::predict(float dist_tiles, float hp_pct,
    int skills_ready) const {
    CloneContext ctx = CloneContext::from_state(dist_tiles, hp_pct, skills_ready);

    ClonePrediction p = _predict_exact(ctx);
    if (p.level >= 0) return p;

    p = _predict_fuzzy(ctx);
    if (p.level >= 0) return p;

    if (_has_profile) {
        p.best = fallback_intent(profile_, dist_tiles, hp_pct);
        p.confidence = 0.5f;
        p.level = 2;
        return p;
    }
    p.best = PlayerIntention::ATTACK;
    p.confidence = 0.5f;
    p.level = 3;
    return p;
}