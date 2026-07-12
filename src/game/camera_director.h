#pragma once

// ============================================================
// D5 Step6: CameraDirector — 统一镜头效果常量
// 所有HitStop/Shake/Zoom/BossIntro/BossDeath从此读取
// ============================================================
struct CameraDirector {
    // HitStop (秒)
    static constexpr float HITSTOP_LIGHT = 0.025f;
    static constexpr float HITSTOP_HEAVY = 0.06f;
    static constexpr float HITSTOP_BOSS  = 0.08f;

    // Shake intensity
    static constexpr float SHAKE_LIGHT  = 2.5f;
    static constexpr float SHAKE_MEDIUM = 6.0f;
    static constexpr float SHAKE_HEAVY  = 12.0f;
    static constexpr float SHAKE_BOSS   = 16.0f;

    // Boss Intro: 暂停时长
    static constexpr float INTRO_FREEZE = 2.0f;
    static constexpr float INTRO_FADE   = 0.8f;

    // Phase2 transition
    static constexpr float PHASE2_PAUSE = 0.5f;
    static constexpr float PHASE2_SHAKE = 10.0f;

    // LastStand
    static constexpr float LASTSTAND_FREEZE = 0.6f;
    static constexpr float LASTSTAND_SHAKE  = 14.0f;

    // Boss Kill (slow-mo + white flash)
    static constexpr float DEATH_SLOWMO = 0.4f;
    static constexpr float DEATH_SHAKE  = 16.0f;
    static constexpr float DEATH_FLASH  = 0.3f;  // 白闪持续
};
