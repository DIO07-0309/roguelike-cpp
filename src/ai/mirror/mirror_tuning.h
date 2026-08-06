#pragma once
// M4: MirrorTuning — F15 实战调参的集中参数表 (单例可调)。
// 所有 Phase 触发阈值 / 仲裁置信度 / 漂移降权集中于此, 便于实测标定。
struct MirrorTuning {
    // ── P1→P2 触发 (观察→镜像) ──
    float phase1_accuracy_threshold = 0.65f;  // 准确率达标
    int   phase1_min_observations   = 20;     // 达标所需最少观察
    int   phase1_obs_backstop       = 40;     // 观察数兜底
    float phase1_time_backstop      = 12.0f;  // 时间兜底 (M4: 20→12, 短战斗也进P2)

    // ── P2→P3 触发 (镜像→进化) ──
    int   phase2_same_bucket_hits   = 10;     // 同桶命中 (核心模式)
    float phase2_accuracy_threshold = 0.70f;
    float phase2_hp_danger          = 0.35f;  // 任一方 HP<35% 濒危直通

    // ── 仲裁 (M3) ──
    float clone_confidence          = 0.50f;  // 克隆层置信度门槛

    // ── 漂移降权 (M2 profile_drift 消费) ──
    float drift_penalty_threshold   = 0.50f;  // 漂移>此值 → 模仿降权
    float drift_penalty_step        = 0.25f;  // 克隆置信门槛上浮量
};

inline MirrorTuning& mirror_tuning() {
    static MirrorTuning t;
    return t;
}
