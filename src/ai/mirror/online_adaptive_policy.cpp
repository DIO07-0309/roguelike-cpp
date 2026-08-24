#include "ai/mirror/online_adaptive_policy.h"
#include "combat_system.h"   // rng()
#include <cmath>
#include <algorithm>

OnlineAdaptivePolicy::OnlineAdaptivePolicy() {
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++) {
            _alpha[b][a] = 1.0f;
            _beta[b][a]  = 1.0f;   // Beta(1,1) = 均匀先验
            _pending_prior[b][a] = 0.0f;
        }
}

void OnlineAdaptivePolicy::init_prior(const PlayerHabitProfile& profile) {
    // 画像 → 先验 (只影响冷启动, 反馈会纠正错误先验)
    // Q3.15 (MP1): 注入量同步记账 — export 前由 strip_pending_prior() 撤销,
    // 否则每局重复固化进存档 → 后验无界增长
    for (int b = 0; b < BUCKET_COUNT; b++) {
        if (profile.predict_low_dodge) {
            _pending_prior[b][(int)MirrorAction::APPROACH] += 2.0f;
            _alpha[b][(int)MirrorAction::APPROACH] += 2.0f;  // 不闪避 → 贴脸压制
        }
        if (profile.predict_attack_heavy) {
            _pending_prior[b][(int)MirrorAction::RETREAT] += 2.0f;
            _beta[b][(int)MirrorAction::RETREAT] += 2.0f;    // 猛攻型 → 少后撤
        }
        if (profile.predict_skill_spam) {
            _pending_prior[b][(int)MirrorAction::SKILL] += 2.0f;
            _alpha[b][(int)MirrorAction::SKILL] += 2.0f;     // 技能流 → 技能对轰
        }
        if (profile.predict_panic_heal) {
            _pending_prior[b][(int)MirrorAction::COMBO] += 1.5f;
            _alpha[b][(int)MirrorAction::COMBO] += 1.5f;     // 乱喝药 → 连招压制
        }
    }
}

int OnlineAdaptivePolicy::bucket_for(float dist_px, float player_hp_pct) {
    int dist_idx = dist_px < 96.0f ? 0 : (dist_px < 192.0f ? 1 : 2);
    int hp_idx   = player_hp_pct < 0.35f ? 0
                : (player_hp_pct < 0.70f ? 1 : 2);
    return dist_idx * 3 + hp_idx;
}

int OnlineAdaptivePolicy::select_action(int bucket) {
    int best = 0;
    double best_sample = -1.0;
    for (int a = 0; a < ACTION_COUNT; a++) {
        double s = _sample_beta(_alpha[bucket][a], _beta[bucket][a]);
        if (s > best_sample) { best_sample = s; best = a; }
    }
    return best;
}

void OnlineAdaptivePolicy::update(int bucket, int action, float reward) {
    if (bucket < 0 || bucket >= BUCKET_COUNT) return;
    if (action < 0 || action >= ACTION_COUNT) return;
    // Q3.15 (MP1 fix): 全臂折扣遗忘 — 非平稳环境(玩家改打法)下旧证据必须降权,
    // 否则后验方差 ∝ 1/(α+β)² 收敛到零 → Boss 永远失去适应力。
    // 桶内总质量稳态 ≈ 1/(1-λ)·1 ≈ 200, 有界; floor 保底防止 Beta 参数退化到 0。
    constexpr float kDiscount = 0.995f;
    constexpr float kFloor = 0.25f;
    for (int a = 0; a < ACTION_COUNT; a++) {
        _alpha[bucket][a] = std::max(kFloor, _alpha[bucket][a] * kDiscount);
        _beta[bucket][a]  = std::max(kFloor, _beta[bucket][a]  * kDiscount);
    }
    _alpha[bucket][action] += reward;
    _beta[bucket][action]  += 1.0f - reward;
}

void OnlineAdaptivePolicy::strip_pending_prior() const {
    // Q3.15 (MP1): 清零本局先验记账 — 先验是"当局冷启动知识",
    // 不应写入跨局记忆 (记忆里只保留战斗反馈学到的真实后验质量)
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++)
            _pending_prior[b][a] = 0.0f;
}

void OnlineAdaptivePolicy::export_alpha(std::vector<float>& out) const {
    out.clear();
    out.reserve(BUCKET_COUNT * ACTION_COUNT);
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++) {
            // Q3.15 (MP1 fix): 导出"扣除画像先验后的纯学习后验" — 否则每局
            // init_prior 的 +2 随存档固化叠加 → 后验无界增长 → 探索归零
            float v = _alpha[b][a];
            if (_pending_prior[b][a] > 0.0f)
                v = std::max(1.0f, v - _pending_prior[b][a]);
            out.push_back(v);
        }
    strip_pending_prior();
}

void OnlineAdaptivePolicy::export_beta(std::vector<float>& out) const {
    out.clear();
    out.reserve(BUCKET_COUNT * ACTION_COUNT);
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++)
            out.push_back(_beta[b][a]);
    strip_pending_prior();
}

void OnlineAdaptivePolicy::import_alpha(const std::vector<float>& in) {
    if (in.size() < (size_t)(BUCKET_COUNT * ACTION_COUNT)) return;
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++)
            _alpha[b][a] += in[(size_t)(b * ACTION_COUNT + a)];
}

void OnlineAdaptivePolicy::import_beta(const std::vector<float>& in) {
    if (in.size() < (size_t)(BUCKET_COUNT * ACTION_COUNT)) return;
    for (int b = 0; b < BUCKET_COUNT; b++)
        for (int a = 0; a < ACTION_COUNT; a++)
            _beta[b][a] += in[(size_t)(b * ACTION_COUNT + a)];
}

float OnlineAdaptivePolicy::win_rate(int bucket, int action) const {
    if (bucket < 0 || bucket >= BUCKET_COUNT) return 0.0f;
    if (action < 0 || action >= ACTION_COUNT) return 0.0f;
    float a = _alpha[bucket][action];
    float b = _beta[bucket][action];
    return a / (a + b);
}

double OnlineAdaptivePolicy::_rand_uniform() {
    return ((double)(rng() % 1000000) + 0.5) / 1000000.0;
}

double OnlineAdaptivePolicy::_rand_gaussian() {
    double u1 = _rand_uniform();
    if (u1 <= 0.0) u1 = 1e-9;
    double u2 = _rand_uniform();
    if (u2 <= 0.0) u2 = 1e-9;
    return std::sqrt(-2.0 * std::log(u1))
         * std::cos(2.0 * 3.14159265358979 * u2);
}

double OnlineAdaptivePolicy::_sample_gamma(float a) {
    // Marsaglia & Tsang (2000): Gamma(a, 1), 参数 a < 1 时提升后递归
    if (a < 1.0f) {
        double u = _rand_uniform();
        if (u <= 0.0) u = 1e-9;
        return _sample_gamma(a + 1.0f) * std::pow(u, 1.0 / (double)a);
    }
    double d = (double)a - 1.0 / 3.0;
    double c = 1.0 / std::sqrt(9.0 * d);
    for (;;) {
        double x = _rand_gaussian();
        double v = 1.0 + c * x;
        if (v <= 0.0) continue;
        v = v * v * v;
        double u = _rand_uniform();
        if (u <= 0.0) continue;
        if (u < 1.0 - 0.0331 * x * x * x * x) return d * v;
        if (std::log(u) < 0.5 * x * x + d * (1.0 - v + std::log(v)))
            return d * v;
    }
}

double OnlineAdaptivePolicy::_sample_beta(float a, float b) {
    double ga = _sample_gamma(a);
    double gb = _sample_gamma(b);
    double sum = ga + gb;
    return sum > 0.0 ? ga / sum : 0.5;
}
