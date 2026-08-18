#include "ai/mirror/mirror_debug_stats.h"
#include <cstdio>

void MirrorDebugStats::reset() { _s = MirrorDebugSnap{}; }

void MirrorDebugStats::on_predict(int level) {
    _s.predict_count++;
    switch (level) {
    case 0: _s.clone_exact++; break;
    case 1: _s.clone_fuzzy++; break;
    case 2: _s.profile_fallback++; break;
    case 3: _s.default_fallback++; break;
    default: _s.rule_fallback++; break;   // 规则兜底分支
    }
}

void MirrorDebugStats::on_arbitrate(bool clone_used, bool ml_used) {
    if (ml_used) _s.ml_slot_used++;
    else if (clone_used) _s.clone_arbitrate++;
    else _s.thompson_used++;
}

void MirrorDebugStats::on_rl() { _s.rl_used++; }

void MirrorDebugStats::on_interrupt(bool success) {
    _s.interrupt_attempt++;
    if (success) _s.interrupt_success++;
}

void MirrorDebugStats::on_behavior_state(int state) {
    switch (state) {
    case 0: _s.behavior_approach++; break;
    case 1: _s.behavior_attack++; break;
    case 2: _s.behavior_skill++; break;
    case 3: _s.behavior_retreat++; break;
    default: break;
    }
}

void MirrorDebugStats::tick_phase(int phase, float dt) {
    if (phase >= 1 && phase <= 3) _s.phase_seconds[phase - 1] += dt;
}

MirrorDebugSnap MirrorDebugStats::snapshot() const { return _s; }

std::string MirrorDebugStats::summary() const {
    const MirrorDebugSnap& s = _s;
    int clone_total = s.clone_exact + s.clone_fuzzy;
    float clone_pct = s.predict_count > 0
        ? 100.0f * (clone_total + s.profile_fallback) / s.predict_count : 0.0f;
    float rule_pct = s.predict_count > 0
        ? 100.0f * (s.rule_fallback + s.default_fallback) / s.predict_count : 0.0f;
    char buf[256];
    snprintf(buf, sizeof(buf),
        "Predict:%d CloneHit:%.0f%%(ex%d/fz%d) Rule:%.0f%% 仲裁[Clone:%d ML:%d RL:%d Tho:%d] "
        "打断%d/%d 行为(A%d/S%d/R%d/App%d) Phase[s:%.0f/%.0f/%.0f]",
        s.predict_count, clone_pct, s.clone_exact, s.clone_fuzzy, rule_pct,
        s.clone_arbitrate, s.ml_slot_used, s.rl_used, s.thompson_used,
        s.interrupt_success, s.interrupt_attempt,
        s.behavior_attack, s.behavior_skill, s.behavior_retreat, s.behavior_approach,
        s.phase_seconds[0], s.phase_seconds[1], s.phase_seconds[2]);
    return std::string(buf);
}