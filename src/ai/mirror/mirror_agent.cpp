#include "ai/mirror/mirror_agent.h"
#include "core/logger.h"
#include "combat_system.h"   // rng()
#include <algorithm>
#include <cstdio>

MirrorAgent::MirrorAgent() = default;

void MirrorAgent::init(const PlayerHabitProfile& profile) {
    _profile = profile;
    _phase = 1;

    // M4e: 创建在线学习策略, 注入离线画像先验
    _policy = std::make_unique<OnlineAdaptivePolicy>();
    _policy->init_prior(profile);
    _last_bucket = -1;
    _last_action = -1;

    // Derive preferred distance from profile
    if (_profile.style == PlayerStyle::AGGRESSIVE)
        _preferred_distance = 180.0f;   // stay mid-close, bait attacks
    else if (_profile.style == PlayerStyle::SNIPER)
        _preferred_distance = 320.0f;   // keep them cornered, close the gap
    else if (_profile.style == PlayerStyle::MAGE)
        _preferred_distance = 250.0f;   // pressure at mid-range, interrupt casts
    else if (_profile.style == PlayerStyle::DEFENSIVE)
        _preferred_distance = 150.0f;   // force them into a corner, no escape
    else
        _preferred_distance = 230.0f;   // balanced
}

const char* MirrorAgent::phase_name() const {
    if (_phase == 1) return "Observe";
    if (_phase == 2) return "Mirror";
    return "Evolve";
}

float MirrorAgent::recommend_distance() const {
    // Phase 1: match player's preferred distance exactly (mirror)
    // Phase 2: adjust slightly to counter
    // Phase 3: actively close/pull based on weakness
    float base = _preferred_distance;
    if (_phase >= 2 && _profile.predict_low_dodge)
        base *= 0.7f;  // pressure closer if they dodge poorly
    if (_phase >= 3 && _profile.predict_attack_heavy)
        base *= 1.25f; // stay safer distance vs hyper-aggressive
    return base;
}

bool MirrorAgent::should_interrupt_skill(const MirrorBattleState& st) const {
    if (_phase < 2) return false;
    // M4e: 玩家正在施放技能 → 立即打断; 或按画像预判技能流
    return st.player_using_skill || _profile.predict_skill_spam;
}

bool MirrorAgent::should_pressure_close(const MirrorBattleState& st) const {
    if (_phase < 2) return false;
    // Pressure close: player HP is low and they tend to heal at this threshold
    if (st.player_hp_pct * 100.0f < _profile.hp_counter_threshold
        && _profile.heal_frequency > 0.01f)
        return true;
    // Phase 3: always pressure if player has low dodge
    if (_phase >= 3 && _profile.predict_low_dodge)
        return true;
    return false;
}

void MirrorAgent::set_clone_table(std::unique_ptr<BehaviorCloneTable> t) {
    _clone = std::move(t);
}

static PlayerActionType intent_to_action(PlayerIntention i) {
    switch (i) {
    case PlayerIntention::ATTACK: return PlayerActionType::ATTACK;
    case PlayerIntention::SKILL:  return PlayerActionType::SKILL;
    case PlayerIntention::DODGE:  return PlayerActionType::DODGE;
    case PlayerIntention::HEAL:   return PlayerActionType::HEAL;
    default:                      return PlayerActionType::NONE;  // no mapping -> rules
    }
}

PlayerActionType MirrorAgent::predict_next_action(
    const MirrorBattleState& st) const {
    // M1: behavior-clone layer first (exact/fuzzy/profile), rules as fallback
    if (_phase >= 2 && _clone) {
        ClonePrediction p = _clone->predict(st.dist_tiles, st.player_hp_pct,
                                            st.player_skills_ready);
        if (p.level <= 2 && p.confidence >= 0.5f) {
            PlayerActionType t = intent_to_action(p.best);
            if (t != PlayerActionType::NONE) return t;
        }
    }
    // Rule-based fallback (existing profile logic)
    if (st.dist_tiles < 3.0f && _profile.attack_frequency > 0.6f)
        return PlayerActionType::ATTACK;
    if (st.player_hp_pct < _profile.hp_counter_threshold / 100.0f
        && _profile.heal_frequency > 0.01f)
        return PlayerActionType::HEAL;
    if (_profile.predict_skill_spam && _phase >= 2)
        return PlayerActionType::SKILL;
    return PlayerActionType::ATTACK;
}

// ── M4e: 在线自适应 — Thompson 采样决策 ──
float MirrorAgent::_rand01() {
    return (float)((rng() % 1000000) / 1000000.0);
}

int MirrorAgent::recommend_action(const MirrorBattleState& st) {
    if (!_policy || _phase < 2) return -1;   // 观察期: 走规则
    float dist_px = st.dist_tiles * 32.0f;
    _last_bucket = OnlineAdaptivePolicy::bucket_for(dist_px, st.player_hp_pct);
    int act = _policy->select_action(_last_bucket);
    // 技能窗口探索: 玩家放技能时 40% 概率尝试技能反制 (反馈落在实际臂上)
    if (st.player_using_skill && act != (int)MirrorAction::SKILL
        && _rand01() < 0.4f)
        act = (int)MirrorAction::SKILL;
    _last_action = act;
    LOG_DEBUG("[MIRROR] 在线决策 phase=%d bucket=%d action=%d (%.0fpx, HP%.0f%%)",
        _phase, _last_bucket, _last_action, dist_px,
        st.player_hp_pct * 100.0f);
    return _last_action;
}

void MirrorAgent::report_outcome(bool hit, float damage) {
    if (!_policy || _last_bucket < 0 || _last_action < 0) return;
    // 命中: 伤害越高奖励越接近1; 落空/被躲: 0
    float reward = hit
        ? std::min(1.0f, 0.6f + 0.4f * std::min(1.0f, damage / 30.0f))
        : 0.0f;
    _policy->update(_last_bucket, _last_action, reward);
    LOG_DEBUG("[MIRROR] 反馈 bucket=%d action=%d hit=%d dmg=%.0f reward=%.2f",
        _last_bucket, _last_action, hit ? 1 : 0, damage, reward);
    _last_bucket = -1;   // 一次决策只允许一次反馈 (防 dodge 重复上报)
    _last_action = -1;
}

void MirrorAgent::export_memory(std::vector<float>& alpha,
                                std::vector<float>& beta) const {
    if (_policy) {
        _policy->export_alpha(alpha);
        _policy->export_beta(beta);
    } else {
        alpha.clear();
        beta.clear();
    }
}

void MirrorAgent::import_memory(const std::vector<float>& alpha,
                                const std::vector<float>& beta_vals) {
    if (!_policy) return;
    _policy->import_alpha(alpha);   // 旧后验 = 新先验 (叠加)
    _policy->import_beta(beta_vals);
}

float MirrorAgent::arm_win_rate(int bucket, int action) const {
    if (!_policy) return 0.0f;
    return _policy->win_rate(bucket, action);
}

// ── M2: 在线观测 ──
void MirrorAgent::on_prediction(PlayerActionType predicted, float dist_tiles,
                                float hp_pct, int skills_ready) {
    if (!is_decision_action(predicted)) return;
    _last_prediction = predicted;
    _last_obs_key = CloneContext::from_state(dist_tiles, hp_pct, skills_ready).key();
}

void MirrorAgent::observe_actual(PlayerActionType actual) {
    if (!is_decision_action(actual)) return;
    _observed_actions++;
    switch (actual) {
    case PlayerActionType::ATTACK: _obs_attack++; break;
    case PlayerActionType::SKILL:  _obs_skill++;  break;
    case PlayerActionType::DODGE:  _obs_dodge++;  break;
    case PlayerActionType::HEAL:   _obs_heal++;   break;
    default: break;
    }
    if (_last_prediction == PlayerActionType::NONE) return;
    bool hit = (actual == _last_prediction);
    _rolling_accuracy.add(hit);
    if (hit) _bucket_hits[_last_obs_key]++;
    _last_prediction = PlayerActionType::NONE;
}

int MirrorAgent::_max_bucket_hits() const {
    int best = 0;
    for (const auto& kv : _bucket_hits) {
        if (kv.second > best) best = kv.second;
    }
    return best;
}

float MirrorAgent::profile_drift() const {
    if (_observed_actions < 10 || _battle_time <= 0.01f) return 0.0f;
    float cur_attack = (float)_obs_attack / _battle_time;
    float cur_skill  = (float)_obs_skill  / _battle_time;
    float attack_norm = _profile.attack_frequency > 0.01f
        ? fabsf(cur_attack - _profile.attack_frequency) / _profile.attack_frequency : 0.0f;
    float skill_norm = _profile.skill_frequency > 0.01f
        ? fabsf(cur_skill - _profile.skill_frequency) / _profile.skill_frequency : 0.0f;
    float drift = (attack_norm + skill_norm) * 0.5f;
    return drift > 1.0f ? 1.0f : drift;
}

void MirrorAgent::tick_phase(float dt, const MirrorBattleState& st) {
    _battle_time += dt;
    switch (_phase) {
    case 1: {
        float acc = _rolling_accuracy.accuracy();
        bool learned = (acc >= 0.65f && _observed_actions >= 20)
                    || _observed_actions >= kPhase1Observations;
        if (learned || _battle_time >= 20.0f) set_phase(2);
        break;
    }
    case 2: {
        bool pattern = _max_bucket_hits() >= 10 && _rolling_accuracy.accuracy() >= 0.7f;
        bool danger  = st.player_hp_pct < 0.35f || st.boss_hp_pct < 0.35f;
        if (pattern || danger) set_phase(3);
        break;
    }
    default: break;
    }
}

// ── F15.4: Mirror reward ──
double MirrorAgent::mirror_reward(const PlayerHabitProfile& profile,
    int /*boss_action*/, double damage_dealt, double damage_taken,
    bool player_dodged, bool player_healed)
{
    double reward = damage_dealt * 0.5 - damage_taken * 0.3;

    // Counter bonuses
    if (profile.predict_panic_heal && player_healed)
        reward += 1.0;  // punished a panic-heal player
    if (profile.predict_low_dodge && player_dodged)
        reward += 0.5;  // predicted and hit a low-dodge player
    if (profile.predict_attack_heavy)
        reward += 0.3;  // aggressive profile = more opportunities to counter

    return reward;
}

int MirrorAgent::style_to_int(PlayerStyle s) {
    switch (s) {
    case PlayerStyle::AGGRESSIVE: return 1;
    case PlayerStyle::DEFENSIVE:  return 2;
    case PlayerStyle::SNIPER:     return 3;
    case PlayerStyle::MAGE:       return 4;
    default: return 0;
    }
}
