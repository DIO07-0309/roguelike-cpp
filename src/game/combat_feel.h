#pragma once

// ============================================================
// D4.6 Step2: CombatFeelSystem — 统一打击感常数
// 所有HitStop/Shake/Crit/DamageNumber从此读取
// ============================================================
struct CombatFeelSystem {
    // HitStop (秒)
    static constexpr float LIGHT_HIT     = 0.025f;
    static constexpr float HEAVY_HIT     = 0.06f;
    static constexpr float BOSS_HIT      = 0.08f;
    static constexpr float CRITICAL_HIT  = 0.05f;
    static constexpr float KILL_SLOWMO   = 0.10f;
    static constexpr float RELIC_PICKUP  = 0.18f;
    static constexpr float BUILD_COMPLETE = 0.15f;

    // CameraShake (像素)
    static constexpr float SHAKE_LIGHT   = 2.5f;
    static constexpr float SHAKE_MEDIUM  = 6.0f;
    static constexpr float SHAKE_HEAVY   = 12.0f;
    static constexpr float SHAKE_BOSS    = 16.0f;

    // Combo里程碑 (hit数)
    static constexpr int COMBO_5   = 5;
    static constexpr int COMBO_10  = 10;
    static constexpr int COMBO_20  = 20;
    static constexpr int COMBO_MAX = 30;

    // 暴击 (combo驱动: 段数越高暴击率越高)
    static float crit_chance(int combo_count) {
        if (combo_count >= 4) return 0.30f;  // 重击 30%
        if (combo_count == 3) return 0.15f;
        if (combo_count == 2) return 0.08f;
        return 0.03f; // combo 1: 3%
    }

    static int crit_multiplier() { return 2; }  // crit = ×2

    // Combo消息
    static const char* combo_message(int count) {
        if (count >= 30) return "PERFECT COMBO!";
        if (count >= 20) return "COMBO ×20!";
        if (count >= 10) return "COMBO ×10!";
        if (count >= 5)  return "5 HIT!";
        return nullptr;
    }
};
